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

#include "Utilities/Errors.h"
#include <algorithm>
#include <string>
#include <list>
#include "Utilities/MathDefines.h"
#include "Unit.h"
#include "combat/MeleeChances.h"
#include "combat/SpellBonus.h"
#include "MotionMaster.h"
#include "State.h"
#include "Log.h"
#include "WorldPacket.h"
#include "data/GameConfig.h"
#include "ObjectMgr.h"
#include "ObjectGuid.h"
#include "spells/data/SpellMgr.h"
#include "QuestDef.h"
#include "Player.h"
#include "Creature.h"
#include "spells/pipeline/Spell.h"
#include "entities/GroupUpdateFacts.h"
#include "entities/MotionNotices.h"
#include "spells/auras/SpellAuras.h"
#include "MapManager.h"
#include "PlayerRegistry.h"
#include "ObjectLookup.h"
#include "CreatureAI.h"
#include "TemporarySummon.h"
#include "Pet.h"
#include "Util.h"
#include "Totem.h"
#include "Vehicle.h"
#include "BattleGround/BattleGround.h"
#include "InstanceData.h"
#include "MapPhase.h"
#include "GridNotifiersImpl.h"
#include "CellImpl.h"
#include "MotionGenerators/MoveSplineInit.h"
#include "movement/MoveSpline.h"
#include "CreatureLinkingMgr.h"
#include "GameTime.h"
#include "Geometry/Placement.h"
#include "Transports.h"
#include "TransportMap.h"
#include "Group.h"

#include <math.h>
#include <stdarg.h>

float baseMoveSpeed[MAX_MOVE_TYPE] =
{
    2.5f,                                                   // MOVE_WALK
    7.0f,                                                   // MOVE_RUN
    4.5f,                                                   // MOVE_RUN_BACK
    4.722222f,                                              // MOVE_SWIM
    2.5f,                                                   // MOVE_SWIM_BACK
    3.141594f,                                              // MOVE_TURN_RATE
    7.0f,                                                   // MOVE_FLIGHT
    4.5f,                                                   // MOVE_FLIGHT_BACK
    3.14f                                                   // MOVE_PITCH_RATE
};

////////////////////////////////////////////////////////////
// Methods of class GlobalCooldownMgr

bool GlobalCooldownMgr::HasGlobalCooldown(SpellEntry const* spellInfo) const
{
    GlobalCooldownList::const_iterator itr = m_GlobalCooldowns.find(spellInfo->GetStartRecoveryCategory());
    return itr != m_GlobalCooldowns.end() && itr->second.duration && getMSTimeDiff(itr->second.cast_time, GameTime::GetGameTimeMS()) < itr->second.duration;
}

/**
 * @brief Starts a global cooldown category timer.
 *
 * @param spellInfo The spell whose recovery category is used.
 * @param gcd The cooldown duration in milliseconds.
 */
void GlobalCooldownMgr::AddGlobalCooldown(SpellEntry const* spellInfo, uint32 gcd)
{
    m_GlobalCooldowns[spellInfo->GetStartRecoveryCategory()] = GlobalCooldown(gcd, GameTime::GetGameTimeMS());
}

/**
 * @brief Cancels an active global cooldown category timer.
 *
 * @param spellInfo The spell whose recovery category is cleared.
 */
void GlobalCooldownMgr::CancelGlobalCooldown(SpellEntry const* spellInfo)
{
    m_GlobalCooldowns[spellInfo->GetStartRecoveryCategory()].duration = 0;
}

////////////////////////////////////////////////////////////
// Methods of class Unit

Unit::Unit() :
    movespline(new Movement::MoveSpline()),
    m_spellCooldownMgr(),
    m_charmInfo(NULL),
    i_motionMaster(std::make_unique<MotionMaster>(this)),
    m_regenTimer(0),
    m_vehicleInfo(NULL),
    m_groupCallbacks(NULL),
    m_clientCallbacks(NULL),
    m_cooldownCallbacks(NULL),
    m_itemCallbacks(NULL),
    m_lastPotionId(0),
    m_lastSwingError(0),
    m_spellMods(NULL),
    m_ThreatManager(this),
    m_HostileRefManager(this),
    m_motion(std::make_unique<Motion::State>(Motion::Mode::ServerDriven, Motion::Kinematics())), m_motionDropped(0), m_moverSession(NULL)
{
    m_objectType |= TYPEMASK_UNIT;
    m_objectTypeId = TYPEID_UNIT;

    m_updateFlag = UPDATEFLAG_LIVING;

    m_attackTimer[BASE_ATTACK]   = 0;
    m_attackTimer[OFF_ATTACK]    = 0;
    m_attackTimer[RANGED_ATTACK] = 0;
    m_modAttackSpeedPct[BASE_ATTACK] = 1.0f;
    m_modAttackSpeedPct[OFF_ATTACK] = 1.0f;
    m_modAttackSpeedPct[RANGED_ATTACK] = 1.0f;

    m_extraAttacks = 0;

    m_state = 0;
    m_deathState = ALIVE;

    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        m_currentSpells[i] = NULL;
    }

    m_castCounter = 0;

    // m_Aura = NULL;
    // m_AurasCheck = 2000;
    // m_removeAuraTimer = 4;
    m_AuraFlags = 0;

    m_Visibility = VISIBILITY_ON;
    m_AINotifyScheduled = false;

    m_detectInvisibilityMask = 0;
    m_invisibilityMask = 0;
    m_transform = 0;
    m_canModifyStats = false;

    for (int i = 0; i < MAX_SPELL_IMMUNITY; ++i)
    {
        m_spellImmune[i].clear();
    }
    for (int i = 0; i < UNIT_MOD_END; ++i)
    {
        m_auraModifiersGroup[i][BASE_VALUE] = 0.0f;
        m_auraModifiersGroup[i][BASE_PCT] = 1.0f;
        m_auraModifiersGroup[i][TOTAL_VALUE] = 0.0f;
        m_auraModifiersGroup[i][TOTAL_PCT] = 1.0f;
    }

    // implement 50% base damage from offhand
    m_auraModifiersGroup[UNIT_MOD_DAMAGE_OFFHAND][TOTAL_PCT] = 0.5f;

    for (int i = 0; i < MAX_ATTACK; ++i)
    {
        m_weaponDamage[i][MINDAMAGE] = BASE_MINDAMAGE;
        m_weaponDamage[i][MAXDAMAGE] = BASE_MAXDAMAGE;
    }
    for (int i = 0; i < MAX_STATS; ++i)
    {
        m_createStats[i] = 0.0f;
    }

    m_attacking = NULL;
    m_modMeleeHitChance = 0.0f;
    m_modRangedHitChance = 0.0f;
    m_modSpellHitChance = 0.0f;
    m_baseSpellCritChance = 5;

    m_CombatTimer = 0;

    // m_victimThreat = 0.0f;
    for (int i = 0; i < MAX_SPELL_SCHOOL; ++i)
    {
        m_threatModifier[i] = 1.0f;
    }
    m_isSorted = true;
    for (int i = 0; i < MAX_MOVE_TYPE; ++i)
    {
        m_speed_rate[i] = 1.0f;
    }

    // The kernel starts from the unit's speeds; the flags are false and the height 0
    // until a setter says otherwise. Server-driven until a Player says it is not.
    *m_motion = Motion::State(Motion::Mode::ServerDriven, InitialKinematics());

    // remove aurastates allowing special moves
    for (int i = 0; i < MAX_REACTIVE; ++i)
    {
        m_reactiveTimer[i] = 0;
    }

    m_isCreatureLinkingTrigger = false;
    m_isSpawningLinked = false;
}

Motion::Kinematics Unit::InitialKinematics() const
{
    Motion::Kinematics k;
    for (int i = 0; i < MAX_MOVE_TYPE; ++i)
    {
        k.speed[i] = GetSpeed(UnitMoveType(i));
    }
    return k;
}

MotionMaster* Unit::GetMotionMaster() { return i_motionMaster.get(); }
MotionMaster const* Unit::GetMotionMaster() const { return i_motionMaster.get(); }

bool Unit::IsStopped() const { return !i_motionMaster->Latches().Moving(); }

bool Unit::FollowLatched() const { return i_motionMaster->Latches().follow; }

void Unit::PropagateSpeedChange() { GetMotionMaster()->PropagateSpeedChange(); }

Motion::State& Unit::MotionState() { AssertMotionOwner(); return *m_motion; }
Motion::State const& Unit::MotionState() const { return *m_motion; }

bool Unit::CanFreeMove() const
{
    // kNoFreeMoveReasons is the old UNIT_STAT_NO_FREE_MOVE less its feign bit, which the
    // published state carries apart (IsFeigningDeath): a real death does not deny free
    // movement, only a feign does.
    return !(GetMotionMaster()->Mobility().reasons & Motion::kNoFreeMoveReasons) &&
           !IsFeigningDeath() && !GetOwnerGuid();
}

bool Unit::Blocked(uint32 reasons) const { return (i_motionMaster->Published().reasons & reasons) != 0; }

bool Unit::IsFeigningDeath() const { return i_motionMaster->Published().feign; }

bool Unit::IsFearedByAura() const { return i_motionMaster->Published().auraFear; }

bool Unit::IsClientMover() const
{
    // The header carries the argument. Two readers only, and both of them ask the same
    // question the kernel's mode does: Aura::HandleAuraModStun's wipe and
    // MotionMaster::ProjectClientRoot's root.
    return GetTypeId() == TYPEID_PLAYER || m_moverSession != NULL;
}

bool Unit::IsOwnedByThisThread() const
{
    Map const* map = GetMap();
    if (MapPhase::Owns(map))
    {
        return true;
    }

    // Inside the phase a thread that updates no map owns none: the vessel is not looked up.
    if (!MapPhase::UpdatesAMap())
    {
        return false;
    }

    // The vessel is looked up only when the unit's own map is not owned.
    Transport const* vessel = Transport::VesselOf(*this);
    return MapPhase::OwnsUnitOn(map, vessel ? vessel->FindMap() : NULL);
}

void Unit::AssertMotionOwner() const
{
    if (!IsInWorld() || IsOwnedByThisThread())
    {
        return;
    }
    MapPhase::Violation("the movement kernel", GetGuidStr().c_str());
#ifdef MANGOS_DEBUG
    MANGOS_ASSERT(false && "movement kernel touched from outside its map's update");
#endif
}

void Unit::AssertSpellSeamOwner() const
{
    if (!IsInWorld() || IsOwnedByThisThread())
    {
        return;
    }
    MapPhase::Violation("the spell seam", GetGuidStr().c_str());
#ifdef MANGOS_DEBUG
    MANGOS_ASSERT(false && "spell seam touched from outside its map's update");
#endif
}

void Unit::SendEmissions(std::vector<Motion::Emission> const& emissions)
{
    AssertMotionOwner();
    if (!IsInWorld())
    {
        return;
    }
    for (size_t i = 0; i < emissions.size(); ++i)
    {
        Motion::Emission const& e = emissions[i];
        switch (e.kind)
        {
            case Motion::EmissionKind::Mover:
                // To the session whose client moves this unit: a player's own, a
                // possessed creature's possessor. None: dropped and counted.
                if (!m_moverSession || !sMotionNotices.MoverTold(*this, *m_moverSession, e.counter, e.change))
                {
                    ++m_motionDropped;
                    continue;
                }
                break;
            case Motion::EmissionKind::Spline:
                if (!sMotionNotices.SplineShown(*this, e.change))
                {
                    ++m_motionDropped;
                    continue;
                }
                break;
            case Motion::EmissionKind::Observer:
                // Everyone but the client that acked it (CPP's SendSpeedChangeToObservers
                // skips the controller the same way).
                if (!sMotionNotices.ObserversTold(*this, e.counter, e.change, m_moverSession ? m_moverSession->GetPlayer() : NULL))
                {
                    ++m_motionDropped;
                    continue;
                }
                break;
        }
    }
}

void Unit::SendMonsterMove(Movement::Aboard const& aboard) const
{
    sMotionNotices.MonsterMoved(*this, aboard);
}

void Unit::SendMonsterMoveStop(Movement::Aboard const& aboard, Movement::Location const& at) const
{
    sMotionNotices.MonsterMoveStopped(*this, aboard, at);
}

Unit::~Unit()
{
    // A session that still names this unit as one it moves would dangle; every path
    // that removes a unit revokes it first (Creature::RemoveFromWorld, LogoutPlayer).
    if (m_moverSession)
    {
        sLog.outError("Unit::~Unit: %s still had a mover session", GetGuidStr().c_str());
    }

    // set current spells as deletable
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        if (m_currentSpells[i])
        {
            m_currentSpells[i]->SetReferencedFromCurrent(false);
            m_currentSpells[i] = NULL;
        }
    }

    delete m_charmInfo;
    delete m_vehicleInfo;
    delete movespline;

    // those should be already removed at "RemoveFromWorld()" call
    MANGOS_ASSERT(m_gameObj.size() == 0);
    MANGOS_ASSERT(m_dynObjGUIDs.size() == 0);
    MANGOS_ASSERT(m_auras.DeferredEmpty());
}

/**
 * @brief Updates spell state, combat timers, and movement for the unit.
 *
 * @param update_diff The elapsed update time in milliseconds for gameplay timers.
 * @param p_time The elapsed time used for movement processing.
 */
void Unit::Update(uint32 update_diff, uint32 p_time)
{
    if (!IsInWorld())
    {
        return;
    }

    /*if (p_time > m_AurasCheck)
    {
    m_AurasCheck = 2000;
    _UpdateAura();
    }else
    m_AurasCheck -= p_time;*/

    // WARNING! Order of execution here is important, do not change.
    // Spells must be processed with event system BEFORE they go to _UpdateSpells.
    // Or else we may have some SPELL_STATE_FINISHED spells stalled in pointers, that is bad.
    m_Events.Update(update_diff);
    _UpdateSpells(update_diff);

    CleanupDeletedAuras();

    // A unit a client moves (a player, a possessed creature) forgets its expired retired
    // counters here. A server-driven unit has nothing pending.
    if (m_motion->GetMode() == Motion::Mode::ClientDriven && m_motion->Pending().Size() > 0)
    {
        m_motion->Tick(GameTime::GetGameTimeMS());
    }

    if (CanHaveThreatList())
    {
        GetThreatManager().UpdateForClient(update_diff);
    }

    // update combat timer only for players and pets
    if (IsInCombat() && GetCharmerOrOwnerPlayerOrPlayerItself())
    {
        // Check melee attacking or chasing (not following, in this case) so pets can reach far away
        // targets without stopping half way there and running off.
        // These flags are reset after target dies or another command is given.
        if (m_HostileRefManager.isEmpty())
        {
            // m_CombatTimer set at aura start and it will be freeze until aura removing
            if (m_CombatTimer <= update_diff)
            {
                CombatStop();
            }
            else
            {
                m_CombatTimer -= update_diff;
            }
        }
    }

    if (uint32 base_att = getAttackTimer(BASE_ATTACK))
    {
        setAttackTimer(BASE_ATTACK, (update_diff >= base_att ? 0 : base_att - update_diff));
    }

    if (uint32 base_att = getAttackTimer(OFF_ATTACK))
    {
        setAttackTimer(OFF_ATTACK, (update_diff >= base_att ? 0 : base_att - update_diff));
    }

    if (IsVehicle())
    {
        // Initialize vehicle if not done
        if (IsAlive() && !m_vehicleInfo->IsInitialized())
        {
            m_vehicleInfo->Initialize();
        }

        // Update passenger positions if we are the first vehicle
        if (!IsBoarded())
        {
            m_vehicleInfo->Update(update_diff);
        }
    }

    // update abilities available only for fraction of time
    UpdateReactives(update_diff);

    if (IsAlive())
    {
        ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, GetHealth() < GetMaxHealth() * 0.20f);
        ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, GetHealth() < GetMaxHealth() * 0.35f);
        ModifyAuraState(AURA_STATE_HEALTH_ABOVE_75_PERCENT, GetHealth() > GetMaxHealth() * 0.75f);
    }

    // A stop took the spline's position: write it now, where a cell switch is safe --
    // unless something else moved the unit since.
    if (m_hasPendingCommit)
    {
        m_hasPendingCommit = false;
        if (Where().X() == m_pendingCommitFrom.x && Where().Y() == m_pendingCommitFrom.y &&
            Where().Z() == m_pendingCommitFrom.z && Where().Facing() == m_pendingCommitFrom.o)
        {
            SetPosition(m_pendingCommit.x, m_pendingCommit.y, m_pendingCommit.z, m_pendingCommit.o);
        }
    }

    UpdateSplineMovement(p_time);
    i_motionMaster->UpdateMotion(p_time);
}

/**
 * @brief Sends a heartbeat movement packet for the unit.
 */
void Unit::WriteMovementInfo(ByteBuffer& out, uint16 opcode, uint32 time) const
{
    // THE ONE PLACE THIS IS DECIDED, and it is decided at the instant of writing, from the
    // map. An ordinary map: send what we have. A transport map: our coordinates ARE that
    // map's, and the client has never heard of it -- no WDT, no terrain, no id it would
    // accept -- so it gets no world position at all. It gets the vessel's guid and those
    // same coordinates as an offset, which is the only thing it can compose a position from.
    Map* on = FindMap();
    TransportMap* hull = on ? on->AsTransport() : NULL;
    Transport* vessel = hull ? hull->Vessel() : NULL;

    if (!vessel)
    {
        // BOTH BRANCHES READ Where(), and the asymmetry was the bug. A pose set by
        // Place().MoveTo -- how anything is teleported, minions across a deck boundary
        // above all -- never reaches m_movementInfo, so writing that struct as-is sent
        // the position the unit held BEFORE the move. Coming off a ship that is a
        // deck-local coordinate in a world position field, and the client walks the pet
        // to (-3, -8, 6) on the continent. Boarding was never affected, because the deck
        // branch below had always taken its numbers from Where().
        MovementInfo ashore = WireStatus();
        ashore.UpdateTime(time);
        ashore.ChangePosition(Where().X(), Where().Y(), Where().Z(), Where().Facing());
        ashore.Write(out, opcode);
        return;
    }

    MovementInfo onDeck = WireStatus();
    onDeck.UpdateTime(time);
    onDeck.SetTransportData(vessel->GetObjectGuid(), Where().X(), Where().Y(), Where().Z(),
                            Where().Facing(), 0, -1);
    onDeck.ChangePosition(0.0f, 0.0f, 0.0f, Where().Facing());
    onDeck.Write(out, opcode);
}

void Unit::SendHeartBeat()
{
    sMotionNotices.HeartBeat(*this);
}

/**
 * @brief Interrupts all current non-melee spells except an optional spell id.
 *
 * @param except_spellid A spell id to keep casting.
 */
void Unit::CastStop(uint32 except_spellid)
{
    for (uint32 i = CURRENT_FIRST_NON_MELEE_SPELL; i < CURRENT_MAX_SPELL; ++i)
        if (m_currentSpells[i] && m_currentSpells[i]->m_spellInfo->ID != except_spellid)
        {
            InterruptSpell(CurrentSpellTypes(i), false);
        }
}

/**
 * @brief Looks up a spell by id and casts it on a unit target.
 *
 * @param Victim The target unit.
 * @param spellId The spell identifier.
 * @param triggered True for triggered casting.
 * @param castItem The casting item, if any.
 * @param triggeredByAura The aura that triggered the cast, if any.
 * @param originalCaster The original caster GUID.
 * @param triggeredBy The triggering spell entry, if any.
 */
void Unit::CastSpell(Unit* Victim, uint32 spellId, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell: unknown spell id %i by caster: %s triggered by aura %u (eff %u)", spellId, GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell: unknown spell id %i by caster: %s", spellId, GetGuidStr().c_str());
        }
        return;
    }

    CastSpell(Victim, spellInfo, triggered, castItem, triggeredByAura, originalCaster, triggeredBy);
}

/**
 * @brief Casts a spell entry on a unit target.
 *
 * @param Victim The target unit.
 * @param spellInfo The spell entry to cast.
 * @param triggered True for triggered casting.
 * @param castItem The casting item, if any.
 * @param triggeredByAura The aura that triggered the cast, if any.
 * @param originalCaster The original caster GUID.
 * @param triggeredBy The triggering spell entry, if any.
 */
void Unit::CastSpell(Unit* Victim, SpellEntry const* spellInfo, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell: unknown spell by caster: %s triggered by aura %u (eff %u)", GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell: unknown spell by caster: %s", GetGuidStr().c_str());
        }
        return;
    }

    if (castItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->ID);
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
        {
            originalCaster = triggeredByAura->GetCasterGuid();
        }

        triggeredBy = triggeredByAura->GetSpellProto();
    }
    else
    {
        triggeredByAura = GetTriggeredByClientAura(spellInfo->ID);
        if (triggeredByAura)
        {
            triggered = true;
            triggeredBy = triggeredByAura->GetSpellProto();
        }
    }

    Spell* spell = new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    SpellCastTargets targets;
    targets.setUnitTarget(Victim);

    if (spellInfo->GetTargets() & TARGET_FLAG_DEST_LOCATION)
    {
        targets.setDestination(Victim->Where().X(), Victim->Where().Y(), Victim->Where().Z());
    }
    if (spellInfo->GetTargets() & TARGET_FLAG_SOURCE_LOCATION)
        if (WorldObject* caster = spell->GetCastingObject())
        {
            targets.setSource(caster->Where().X(), caster->Where().Y(), caster->Where().Z());
        }

    spell->m_CastItem = castItem;
    spell->SpellStart(&targets, triggeredByAura);
}

/**
 * @brief Looks up a spell by id and casts it with custom base points.
 *
 * @param Victim The target unit.
 * @param spellId The spell identifier.
 * @param bp0 Optional custom base points for effect 0.
 * @param bp1 Optional custom base points for effect 1.
 * @param bp2 Optional custom base points for effect 2.
 * @param triggered True for triggered casting.
 * @param castItem The casting item, if any.
 * @param triggeredByAura The aura that triggered the cast, if any.
 * @param originalCaster The original caster GUID.
 * @param triggeredBy The triggering spell entry, if any.
 */
void Unit::CastCustomSpell(Unit* Victim, uint32 spellId, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastCustomSpell: unknown spell id %i by caster: %s triggered by aura %u (eff %u)", spellId, GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastCustomSpell: unknown spell id %i by caster: %s", spellId, GetGuidStr().c_str());
        }
        return;
    }

    CastCustomSpell(Victim, spellInfo, bp0, bp1, bp2, triggered, castItem, triggeredByAura, originalCaster, triggeredBy);
}

/**
 * @brief Casts a spell entry with custom base points.
 *
 * @param Victim The target unit.
 * @param spellInfo The spell entry to cast.
 * @param bp0 Optional custom base points for effect 0.
 * @param bp1 Optional custom base points for effect 1.
 * @param bp2 Optional custom base points for effect 2.
 * @param triggered True for triggered casting.
 * @param castItem The casting item, if any.
 * @param triggeredByAura The aura that triggered the cast, if any.
 * @param originalCaster The original caster GUID.
 * @param triggeredBy The triggering spell entry, if any.
 */
void Unit::CastCustomSpell(Unit* Victim, SpellEntry const* spellInfo, int32 const* bp0, int32 const* bp1, int32 const* bp2, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastCustomSpell: unknown spell by caster: %s triggered by aura %u (eff %u)", GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastCustomSpell: unknown spell by caster: %s", GetGuidStr().c_str());
        }
        return;
    }

    if (castItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->ID);
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
        {
            originalCaster = triggeredByAura->GetCasterGuid();
        }

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    Spell* spell = new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    if (bp0)
    {
        spell->m_currentBasePoints[EFFECT_INDEX_0] = *bp0;
    }

    if (bp1)
    {
        spell->m_currentBasePoints[EFFECT_INDEX_1] = *bp1;
    }

    if (bp2)
    {
        spell->m_currentBasePoints[EFFECT_INDEX_2] = *bp2;
    }

    SpellCastTargets targets;
    targets.setUnitTarget(Victim);
    spell->m_CastItem = castItem;

    if (spellInfo->GetTargets() & TARGET_FLAG_DEST_LOCATION)
    {
        targets.setDestination(Victim->Where().X(), Victim->Where().Y(), Victim->Where().Z());
    }
    if (spellInfo->GetTargets() & TARGET_FLAG_SOURCE_LOCATION)
        if (WorldObject* caster = spell->GetCastingObject())
        {
            targets.setSource(caster->Where().X(), caster->Where().Y(), caster->Where().Z());
        }

    spell->SpellStart(&targets, triggeredByAura);
}

// used for scripting
void Unit::CastSpell(float x, float y, float z, uint32 spellId, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell(x,y,z): unknown spell id %i by caster: %s triggered by aura %u (eff %u)", spellId, GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell(x,y,z): unknown spell id %i by caster: %s", spellId, GetGuidStr().c_str());
        }
        return;
    }

    CastSpell(x, y, z, spellInfo, triggered, castItem, triggeredByAura, originalCaster, triggeredBy);
}

// used for scripting
void Unit::CastSpell(float x, float y, float z, SpellEntry const* spellInfo, bool triggered, Item* castItem, Aura* triggeredByAura, ObjectGuid originalCaster, SpellEntry const* triggeredBy)
{
    if (!spellInfo)
    {
        if (triggeredByAura)
        {
            sLog.outError("CastSpell(x,y,z): unknown spell by caster: %s triggered by aura %u (eff %u)", GetGuidStr().c_str(), triggeredByAura->GetId(), triggeredByAura->GetEffIndex());
        }
        else
        {
            sLog.outError("CastSpell(x,y,z): unknown spell by caster: %s", GetGuidStr().c_str());
        }
        return;
    }

    if (castItem)
    {
        DEBUG_FILTER_LOG(LOG_FILTER_SPELL_CAST, "WORLD: cast Item spellId - %i", spellInfo->ID);
    }

    if (triggeredByAura)
    {
        if (!originalCaster)
        {
            originalCaster = triggeredByAura->GetCasterGuid();
        }

        triggeredBy = triggeredByAura->GetSpellProto();
    }

    Spell* spell = new Spell(this, spellInfo, triggered, originalCaster, triggeredBy);

    SpellCastTargets targets;

    if (spellInfo->GetTargets() & TARGET_FLAG_DEST_LOCATION)
    {
        targets.setDestination(x, y, z);
    }
    if (spellInfo->GetTargets() & TARGET_FLAG_SOURCE_LOCATION)
    {
        targets.setSource(x, y, z);
    }

    // Spell cast with x,y,z but without dbc target-mask, set destination
    if (!(targets.m_targetMask & (TARGET_FLAG_DEST_LOCATION | TARGET_FLAG_SOURCE_LOCATION)))
    {
        targets.setDestination(x, y, z);
    }

    spell->m_CastItem = castItem;
    spell->SpellStart(&targets, triggeredByAura);
}


/**
 * @brief Sends a one-shot emote command to nearby clients.
 *
 * @param emote_id The emote identifier.
 */
void Unit::HandleEmoteCommand(uint32 emote_id)
{
    DEBUG_LOG("SMSG_EMOTE %u", emote_id);
    WorldPacket data(SMSG_EMOTE, 4 + 8);
    data << uint32(emote_id);
    data << GetObjectGuid();
    SendMessageToSet(&data, true);
}

/**
 * @brief Sets the unit's persistent emote state.
 *
 * @param emote_id The emote state identifier.
 */
void Unit::HandleEmoteState(uint32 emote_id)
{
    SetUInt32Value(UNIT_NPC_EMOTESTATE, emote_id);
}

/**
 * @brief Handles an emote by dispatching it as a command or state.
 *
 * @param emote_id The emote identifier.
 */
void Unit::HandleEmote(uint32 emote_id)
{
    if (!emote_id)
    {
        HandleEmoteState(0);
    }
    else if (EmotesEntry const* emoteEntry = sEmotesStore.LookupEntry(emote_id))
    {
        if (emoteEntry->EmoteSpecProc)                          // 1,2 states, 0 command
        {
            HandleEmoteState(emote_id);
        }
        else
        {
            HandleEmoteCommand(emote_id);
        }
    }
}

namespace
{
    /// Posts the update of `holder` on `target` for the world thread, and says whether it did: when
    /// its caster is another player, in the world, whom this thread does not own.
    bool PostOffMapAuraUpdate(Unit const& target, SpellAuraHolder const& holder, uint32 diff)
    {
        ObjectGuid const casterGuid = holder.GetCasterGuid();
        if (!casterGuid.IsPlayer() || casterGuid == target.GetObjectGuid())
        {
            return false;
        }

        Unit const* caster = holder.GetCaster();
        if (!caster || !caster->IsInWorld())
        {
            return false;
        }

        Transport const* vessel = Transport::VesselOf(*caster);
        if (!OffMapAuraUpdates::Waits(caster->GetMap(), vessel ? vessel->FindMap() : NULL))
        {
            return false;
        }

        OffMapAuraUpdate update;
        update.mapId = target.GetMapId();
        update.instanceId = target.GetInstanceId();
        update.target = target.GetObjectGuid().GetRawValue();
        update.spellId = holder.GetId();
        update.caster = casterGuid.GetRawValue();
        update.diff = diff;
        OffMapAuraUpdates::Post(update);
        return true;
    }
}

/**
 * @brief Updates active spells, auras, and owned game objects.
 *
 * @param time The elapsed update time in milliseconds.
 */
void Unit::_UpdateSpells(uint32 time)
{
    if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
    {
        _UpdateAutoRepeatSpell();
    }

    // remove finished spells from current pointers
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
    {
        if (m_currentSpells[i] && m_currentSpells[i]->getState() == SPELL_STATE_FINISHED)
        {
            m_currentSpells[i]->SetReferencedFromCurrent(false);
            m_currentSpells[i] = NULL;                      // remove pointer
        }
    }

    // update auras
    // the container's update iterator can be moved on by AuraContainer::EraseHolder from indirectly
    // called code at aura remove, to skip an entry that was planned to update but has been removed
    for (SpellAuraHolder* i_holder = m_auras.BeginUpdate(); i_holder; i_holder = m_auras.NextUpdate())
    {
        if (PostOffMapAuraUpdate(*this, *i_holder, time))
        {
            continue;                                       // the world thread runs it after the map phase, expiry included
        }
        i_holder->UpdateHolder(time);                       // the walk already shifted to the next element, so a removal from in here is safe
    }

    // remove expired auras
    for (SpellAuraHolderMap::iterator iter = m_auras.Holders().begin(); iter != m_auras.Holders().end();)
    {
        SpellAuraHolder* holder = iter->second;

        if (!(holder->IsPermanent() || holder->IsPassive()) && holder->GetAuraDuration() == 0)
        {
            RemoveSpellAuraHolder(holder, AURA_REMOVE_BY_EXPIRE);
            iter = m_auras.Holders().begin();
        }
        else
        {
            ++iter;
        }
    }

    if (!m_gameObj.empty())
    {
        GameObjectList::iterator ite1, dnext1;
        for (ite1 = m_gameObj.begin(); ite1 != m_gameObj.end(); ite1 = dnext1)
        {
            dnext1 = ite1;
            //(*i)->Update( difftime );
            if (!(*ite1)->isSpawned())
            {
                (*ite1)->SetOwnerGuid(ObjectGuid());
                (*ite1)->SetRespawnTime(0);
                (*ite1)->Delete();
                dnext1 = m_gameObj.erase(ite1);
            }
            else
            {
                ++dnext1;
            }
        }
    }
}

/**
 * @brief Updates the unit's auto-repeat spell casting state.
 */
void Unit::_UpdateAutoRepeatSpell()
{
    bool isAutoShot = m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->ID == SPELL_ID_AUTOSHOT;

    // check movement
    if (GetTypeId() == TYPEID_PLAYER && isMoving() &&
        !HasAffectedAura(SPELL_AURA_ALLOW_CAST_WHILE_MOVING, m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo))
    {
        // cancel wand shoot
        if (!isAutoShot)
        {
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
        }
        // auto shot just waits
        return;
    }

    // check spell casts
    if (IsNonMeleeSpellCasted(false, false, true))
    {
        // cancel wand shoot
        if (!isAutoShot)
        {
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            return;
        }
        // auto shot is delayed by everythihng, except ranged(!) CURRENT_GENERIC_SPELL's -> recheck that
        else if (!(m_currentSpells[CURRENT_GENERIC_SPELL] && m_currentSpells[CURRENT_GENERIC_SPELL]->IsRangedSpell()))
        {
            return;
        }
    }

    // castroutine
    if (isAttackReady(RANGED_ATTACK))
    {
        // Check if able to cast
        if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->CheckCast(true) != SPELL_CAST_OK)
        {
            InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
            return;
        }

        // we want to shoot
        Spell* spell = new Spell(this, m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo, true);
        spell->SpellStart(&(m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_targets));

        // all went good, reset attack
        resetAttackTimer(RANGED_ATTACK);
    }
}

/**
 * @brief Registers a spell as the current spell for its casting slot.
 *
 * @param pSpell The spell to assign as current.
 */
void Unit::SetCurrentCastedSpell(Spell* pSpell)
{
    MANGOS_ASSERT(pSpell);                                  // NULL may be never passed here, use InterruptSpell or InterruptNonMeleeSpells

    CurrentSpellTypes CSpellType = pSpell->GetCurrentContainer();

    if (pSpell == m_currentSpells[CSpellType])
    {
        return;       // avoid breaking self
    }

    // break same type spell if it is not delayed
    InterruptSpell(CSpellType, false);

    // special breakage effects:
    switch (CSpellType)
    {
        case CURRENT_GENERIC_SPELL:
        {
            // generic spells always break channeled not delayed spells
            InterruptSpell(CURRENT_CHANNELED_SPELL, false);

            // autorepeat breaking
            if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
            {
                // break autorepeat if not Auto Shot
                if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->ID != SPELL_ID_AUTOSHOT)
                {
                    InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
                }
            }
        } break;

        case CURRENT_CHANNELED_SPELL:
        {
            // channel spells always break generic non-delayed and any channeled spells
            InterruptSpell(CURRENT_GENERIC_SPELL, false);
            InterruptSpell(CURRENT_CHANNELED_SPELL);

            // it also does break autorepeat if not Auto Shot
            if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL] &&
                    m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->ID != SPELL_ID_AUTOSHOT)
                    {
                        InterruptSpell(CURRENT_AUTOREPEAT_SPELL);
                    }
        } break;

        case CURRENT_AUTOREPEAT_SPELL:
        {
            // only Auto Shoot does not break anything
            if (pSpell->m_spellInfo->ID != SPELL_ID_AUTOSHOT)
            {
                // generic autorepeats break generic non-delayed and channeled non-delayed spells
                InterruptSpell(CURRENT_GENERIC_SPELL, false);
                InterruptSpell(CURRENT_CHANNELED_SPELL, false);
                // special action: first cast delay
                if (getAttackTimer(RANGED_ATTACK) < 500)
                {
                    setAttackTimer(RANGED_ATTACK, 500);
                }
            }
        } break;

        default:
        {
            // other spell types don't break anything now
        } break;
    }

    // current spell (if it is still here) may be safely deleted now
    if (m_currentSpells[CSpellType])
    {
        m_currentSpells[CSpellType]->SetReferencedFromCurrent(false);
    }

    // set new current spell
    m_currentSpells[CSpellType] = pSpell;
    pSpell->SetReferencedFromCurrent(true);

    pSpell->SetSelfContainer(&(m_currentSpells[pSpell->GetCurrentContainer()]));
    // previous and faulty version of the following code. If the above proves to work, then delete this instruction
    //   pSpell->m_selfContainer = &(m_currentSpells[pSpell->GetCurrentContainer()]);
}

/**
 * @brief Interrupts a current spell in the specified spell slot.
 *
 * @param spellType The current spell slot to interrupt.
 * @param withDelayed True to also interrupt delayed spells.
 */
void Unit::InterruptSpell(CurrentSpellTypes spellType, bool withDelayed, bool sendAutoRepeatCancelToClient)
{
    MANGOS_ASSERT(spellType < CURRENT_MAX_SPELL);

    if (m_currentSpells[spellType] && (withDelayed || m_currentSpells[spellType]->getState() != SPELL_STATE_DELAYED))
    {
        // send autorepeat cancel message for autorepeat spells
        if (spellType == CURRENT_AUTOREPEAT_SPELL && sendAutoRepeatCancelToClient)
        {
            if (GetTypeId() == TYPEID_PLAYER)
            {
                SendAutoRepeatCancel(this);
            }
        }

        if (m_currentSpells[spellType]->getState() != SPELL_STATE_FINISHED)
        {
            m_currentSpells[spellType]->cancel();
        }

        // cancel can interrupt spell already (caster cancel ->target aura remove -> caster iterrupt)
        if (m_currentSpells[spellType])
        {
            m_currentSpells[spellType]->SetReferencedFromCurrent(false);
            m_currentSpells[spellType] = NULL;
        }
    }
}

/**
 * @brief Finalizes a current spell in the specified spell slot.
 *
 * @param spellType The current spell slot to finish.
 * @param ok True when the spell completed successfully.
 */
void Unit::FinishSpell(CurrentSpellTypes spellType, bool ok /*= true*/)
{
    Spell* spell = m_currentSpells[spellType];
    if (!spell)
    {
        return;
    }

    if (spellType == CURRENT_CHANNELED_SPELL)
    {
        spell->SendChannelUpdate(0);
    }

    spell->finish(ok);
}

/**
 * @brief Checks whether the unit is currently casting a non-melee spell.
 *
 * @param withDelayed True to treat delayed spells as active casts.
 * @param skipChanneled True to ignore channeled spells.
 * @param skipAutorepeat True to ignore auto-repeat spells.
 * @return True if a matching non-melee spell is being cast; otherwise, false.
 */
bool Unit::IsNonMeleeSpellCasted(bool withDelayed, bool skipChanneled, bool skipAutorepeat) const
{
    // We don't do loop here to explicitly show that melee spell is excluded.
    // Maybe later some special spells will be excluded too.

    // generic spells are casted when they are not finished and not delayed
    if (m_currentSpells[CURRENT_GENERIC_SPELL] &&
        (m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_FINISHED) &&
        (withDelayed || m_currentSpells[CURRENT_GENERIC_SPELL]->getState() != SPELL_STATE_DELAYED))
    {
        return true;
    }

    // channeled spells may be delayed, but they are still considered casted
    else if (!skipChanneled && m_currentSpells[CURRENT_CHANNELED_SPELL] &&
             (m_currentSpells[CURRENT_CHANNELED_SPELL]->getState() != SPELL_STATE_FINISHED))
    {
        return true;
    }
    // autorepeat spells may be finished or delayed, but they are still considered casted
    else if (!skipAutorepeat && m_currentSpells[CURRENT_AUTOREPEAT_SPELL])
    {
        return true;
    }

    return false;
}

/**
 * @brief Interrupts current non-melee spells, optionally filtered by spell id.
 *
 * @param withDelayed True to also interrupt delayed spells.
 * @param spell_id An optional spell id filter.
 */
void Unit::InterruptNonMeleeSpells(bool withDelayed, uint32 spell_id)
{
    // generic spells are interrupted if they are not finished or delayed
    if (m_currentSpells[CURRENT_GENERIC_SPELL] && (!spell_id || m_currentSpells[CURRENT_GENERIC_SPELL]->m_spellInfo->ID == spell_id))
    {
        InterruptSpell(CURRENT_GENERIC_SPELL, withDelayed);
    }

    // autorepeat spells are interrupted if they are not finished or delayed
    if (m_currentSpells[CURRENT_AUTOREPEAT_SPELL] && (!spell_id || m_currentSpells[CURRENT_AUTOREPEAT_SPELL]->m_spellInfo->ID == spell_id))
    {
        InterruptSpell(CURRENT_AUTOREPEAT_SPELL, withDelayed);
    }

    // channeled spells are interrupted if they are not finished, even if they are delayed
    if (m_currentSpells[CURRENT_CHANNELED_SPELL] && (!spell_id || m_currentSpells[CURRENT_CHANNELED_SPELL]->m_spellInfo->ID == spell_id))
    {
        InterruptSpell(CURRENT_CHANNELED_SPELL, true);
    }
}

/**
 * @brief Finds a current spell by spell identifier.
 *
 * @param spell_id The spell identifier to search for.
 * @return The matching current spell, or NULL if none exists.
 */
Spell* Unit::FindCurrentSpellBySpellId(uint32 spell_id) const
{
    for (uint32 i = 0; i < CURRENT_MAX_SPELL; ++i)
        if (m_currentSpells[i] && m_currentSpells[i]->m_spellInfo->ID == spell_id)
        {
            return m_currentSpells[i];
        }
    return NULL;
}

namespace
{
    /// What SpellCooldownMgr::AddSpellAndCategoryCooldowns needs from the player, read just
    /// before the call. The spell mods apply to the value the manager hands them.
    SpellCooldownMgr::CastInputs ReadCastInputs(Unit* unit, SpellEntry const* spellInfo)
    {
        SpellCooldownMgr::CastInputs inputs;
        inputs.itemPrototype = ObjectMgr::GetItemPrototype;
        inputs.autoRepeatRanged = IsAutoRepeatRangedSpell(spellInfo);
        inputs.rangedAttackTime = unit->GetAttackTime(RANGED_ATTACK);
        inputs.applyCooldownMod = [unit](uint32 spellId, int32& cooldown)
        {
            unit->GetSpellMods()->ApplySpellMod(spellId, SPELLMOD_COOLDOWN, cooldown);
        };
        return inputs;
    }

    /// The one-spell clear SpellCooldownMgr::RemoveSpellCooldown reports: that spell's cooldown
    /// cleared, through the player's cleared-cooldowns callback.
    SpellCooldownMgr::ClearSink ClearCooldownSink(Unit* unit, CooldownSinks const* sinks)
    {
        return [unit, sinks](uint32 spellId)
        {
            CooldownsClearedFact fact;
            fact.owner = unit->GetObjectGuid();
            fact.spellIds.push_back(spellId);
            MANGOS_ASSERT(sinks->cleared);
            sinks->cleared(fact);
        };
    }
}

/**
 * @brief Starts a player's spell cooldown and its category's.
 *
 * @param spellInfo The spell.
 * @param itemId The item that cast it, 0 for none.
 * @param infinityCooldown True to hold the cooldown until it is removed.
 */
void Unit::AddSpellAndCategoryCooldowns(SpellEntry const* spellInfo, uint32 itemId, Spell* /*spell*/, bool infinityCooldown)
{
    if (!m_cooldownCallbacks)
    {
        return;
    }

    time_t now = time(NULL);
    SpellCooldownMgr::CastInputs const inputs = ReadCastInputs(this, spellInfo);

    m_spellCooldownMgr.AddSpellAndCategoryCooldowns(spellInfo, itemId, now, inputs, infinityCooldown);
}

/**
 * @brief Starts a player's spell cooldown and its category's and reports the cooldown event.
 *
 * @param spellInfo The spell.
 * @param itemId The item that cast it, 0 for none.
 */
void Unit::SendCooldownEvent(SpellEntry const* spellInfo, uint32 itemId, Spell* /*spell*/)
{
    if (!m_cooldownCallbacks)
    {
        return;
    }

    time_t now = time(NULL);
    SpellCooldownMgr::CastInputs const inputs = ReadCastInputs(this, spellInfo);

    m_spellCooldownMgr.SendCooldownEvent(spellInfo, itemId, now, inputs, GetObjectGuid(), m_cooldownCallbacks->event);
}

/**
 * @brief Ends a player's spell cooldown.
 *
 * @param spell_id The spell.
 * @param update True to tell the player's client.
 */
void Unit::RemoveSpellCooldown(uint32 spell_id, bool update)
{
    if (!m_cooldownCallbacks)
    {
        return;
    }

    m_spellCooldownMgr.RemoveSpellCooldown(spell_id, update, ClearCooldownSink(this, m_cooldownCallbacks));
}

/**
 * @brief Ends the cooldown of every spell of a category for a player.
 *
 * @param cat The spell category.
 * @param update True to tell the player's client.
 */
void Unit::RemoveSpellCategoryCooldown(uint32 cat, bool update)
{
    if (!m_cooldownCallbacks)
    {
        return;
    }

    m_spellCooldownMgr.RemoveSpellCategoryCooldown(cat, update, ClearCooldownSink(this, m_cooldownCallbacks));
}

/**
 * @brief Starts the cooldown of the potion a player used in combat, once combat has ended.
 *
 * @param spell The potion's cast, NULL when combat ends.
 */
void Unit::UpdatePotionCooldown(Spell* spell)
{
    if (!m_cooldownCallbacks)
    {
        return;
    }

    // no potion used in combat or still in combat
    if (!m_lastPotionId || IsInCombat())
    {
        return;
    }

    // Call not from spell cast, send cooldown event for item spells if no in combat
    if (!spell)
    {
        // spell/item pair let set proper cooldown (except nonexistent charged spell cooldown spellmods for potions)
        if (ItemPrototype const* proto = ObjectMgr::GetItemPrototype(m_lastPotionId))
            for (int idx = 0; idx < 5; ++idx)
                if (proto->Spells[idx].SpellId && proto->Spells[idx].SpellTrigger == ITEM_SPELLTRIGGER_ON_USE)
                    if (SpellEntry const* spellInfo = sSpellStore.LookupEntry(proto->Spells[idx].SpellId))
                    {
                        SendCooldownEvent(spellInfo, m_lastPotionId);
                    }
    }
    // from spell cases (m_lastPotionId set in Spell::SendSpellCooldown)
    else
    {
        SendCooldownEvent(spell->m_spellInfo, m_lastPotionId, spell);
    }

    m_lastPotionId = 0;
}

/**
 * @brief Rotates the unit to face another unit immediately.
 *
 * @param target The unit to face.
 */
void Unit::SetInFront(Unit const* target)
{
    Place().Face(Where().BearingTo(target->Where()));
}

/**
 * @brief Starts a spline turn toward an orientation.
 *
 * @param ori The target orientation.
 */
void Unit::SetFacingTo(float ori)
{
    Movement::MoveSplineInit init(*this);
    init.SetFacing(ori);
    init.Launch();
}

/**
 * @brief Faces a world object when the unit is currently stopped.
 *
 * @param pObject The object to face.
 */
void Unit::SetFacingToObject(WorldObject* pObject)
{
    // never face when already moving
    if (!IsStopped())
    {
        return;
    }

    // TODO: figure out under what conditions creature will move towards object instead of facing it where it currently is.
    SetFacingTo(Where().BearingTo(pObject->Where()));
}

/**
 * @brief Checks whether this unit is in a place a creature can access.
 *
 * @param c The creature whose movement abilities are checked.
 * @return True if the creature can access the unit's location; otherwise, false.
 */
bool Unit::isInAccessablePlaceFor(Creature const* c) const
{
    if (IsInWater())
    {
        return c->CanSwim();
    }
    else
    {
        return c->CanWalk() || c->CanFly();
    }
}

/**
 * @brief Checks whether the unit is currently in water.
 *
 * @return True if the unit is in water; otherwise, false.
 */
bool Unit::IsInWater() const
{
    return GetTerrain()->IsInWater(Where().X(), Where().Y(), Where().Z());
}

/**
 * @brief Checks whether the unit is currently underwater.
 *
 * @return True if the unit is underwater; otherwise, false.
 */
bool Unit::IsUnderWater() const
{
    return GetTerrain()->IsUnderWater(Where().X(), Where().Y(), Where().Z());
}

/**
 * @brief Restores the unit's native display id.
 */
void Unit::DeMorph()
{
    SetDisplayId(GetNativeDisplayId());
}

/**
 * @brief Changes the unit's power type and updates dependent state.
 *
 * @param new_powertype The new power type.
 */
void Unit::SetPowerType(Powers new_powertype)
{
    // set power type
    SetByteValue(UNIT_FIELD_BYTES_0, 3, new_powertype);

    // group updates
    if (GetTypeId() == TYPEID_PLAYER)
    {
        ReportGroupFact(InstalledGroupCallbacks(m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_POWER_TYPE});
    }
    else if (Pet* pet = ToPet(this))
    {
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER))
            {
                ReportGroupFact(InstalledGroupCallbacks(owner->m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_PET_POWER_TYPE});
            }
        }
    }

    // special cases for power type switching (druid and pets only)
    if (GetTypeId() == TYPEID_PLAYER || IsPet())
    {
        uint32 maxValue = GetCreatePowers(new_powertype);
        uint32 curValue = maxValue;

        // special cases with current power = 0
        switch (new_powertype)
        {
            case POWER_RAGE:
            case POWER_RUNE:
            case POWER_RUNIC_POWER:
                curValue = 0;
                break;
            default:
                break;
        }

        // set power (except for mana)
        if (new_powertype != POWER_MANA)
        {
            SetMaxPower(new_powertype, maxValue);
            SetPower(new_powertype, curValue);
        }

        // send power type update to client
        WorldPacket data(SMSG_POWER_UPDATE);
        data << GetPackGUID();
        data << uint32(1);              // power count
        data << uint8(new_powertype);
        data << uint32(curValue);
        SendMessageToSet(&data, true);
    }
}

bool Unit::CanAttackByItself() const
{
    if (!IsVehicle())
    {
        return true;
    }

    for (uint8 i = 0; i < MAX_VEHICLE_SEAT; ++i)
    {
        if (uint32 seatId = m_vehicleInfo->GetVehicleEntry()->SeatID[i])
        {
            if (VehicleSeatEntry const* seatEntry = sVehicleSeatStore.LookupEntry(seatId))
            {
                if (seatEntry->Flags & SEAT_FLAG_CAN_CONTROL)
                {
                    return false;
                }
            }
        }
    }

    return true;
}

/**
 * @brief Gets the owner unit, if any.
 *
 * @return The owner unit, or NULL if none exists.
 */
Unit* Unit::GetOwner() const
{
    if (ObjectGuid ownerid = GetOwnerGuid())
    {
        return ObjectLookup::GetUnit(*this, ownerid);
    }
    return NULL;
}

/**
 * @brief Gets the charmer unit, if any.
 *
 * @return The charmer unit, or NULL if none exists.
 */
Unit* Unit::GetCharmer() const
{
    if (ObjectGuid charmerid = GetCharmerGuid())
    {
        return ObjectLookup::GetUnit(*this, charmerid);
    }
    return NULL;
}

/**
 * @brief Checks whether the unit or its charmer or owner is a player.
 *
 * @return True if player-controlled through self, owner, or charmer; otherwise, false.
 */
bool Unit::IsCharmerOrOwnerPlayerOrPlayerItself() const
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        return true;
    }

    return GetCharmerOrOwnerGuid().IsPlayer();
}

/**
 * @brief Gets the controlling player through self, charmer, or owner.
 *
 * @return The controlling player, or NULL if none exists.
 */
Player* Unit::GetCharmerOrOwnerPlayerOrPlayerItself()
{
    ObjectGuid guid = GetCharmerOrOwnerGuid();
    if (guid.IsPlayer())
    {
        return sPlayerRegistry.Find(guid);
    }

    return ToPlayer();
}

/**
 * @brief Gets the controlling player through self, charmer, or owner.
 *
 * @return The controlling player, or NULL if none exists.
 */
Player const* Unit::GetCharmerOrOwnerPlayerOrPlayerItself() const
{
    ObjectGuid guid = GetCharmerOrOwnerGuid();
    if (guid.IsPlayer())
    {
        return sPlayerRegistry.Find(guid);
    }

    return ToPlayer();
}

/**
 * @brief Gets the unit's active pet, if any.
 *
 * @return The active pet, or NULL if none exists.
 */
Pet* Unit::GetPet() const
{
    if (ObjectGuid pet_guid = GetPetGuid())
    {
        Map* on = FindMap();
        if (!on)
        {
            return NULL;
        }

        if (Pet* pet = on->GetPet(pet_guid))
        {
            return pet;
        }

        // ACROSS THE DECK BOUNDARY. A master and his minion stand on two maps for as long
        // as one of them has crossed and the other has not, and the owner's map alone
        // cannot find it. Giving up here did not lose the pet -- it was alive the whole
        // time, on the other side -- it made the OWNER FORGET IT, because the line below
        // clears the guid, and after that nothing ever reclaims it.
        if (TransportMap* hull = on->AsTransport())
        {
            Transport* vessel = hull->Vessel();
            if (Map* sailed = vessel ? vessel->FindMap() : NULL)
            {
                if (Pet* pet = sailed->GetPet(pet_guid))
                {
                    return pet;
                }
            }
        }
        else
        {
            MapManager::TransportsByMapType::const_iterator vessels =
                sMapMgr.m_TransportsByMap.find(on->GetId());
            if (vessels != sMapMgr.m_TransportsByMap.end())
            {
                for (Transport* vessel : vessels->second)
                {
                    if (TransportMap* deck = vessel->AsMap())
                    {
                        if (Pet* pet = deck->GetPet(pet_guid))
                        {
                            return pet;
                        }
                    }
                }
            }
        }

        sLog.outError("Unit::GetPet: %s not exist.", pet_guid.GetString().c_str());
        const_cast<Unit*>(this)->SetPet(0);
    }

    return NULL;
}

/**
 * @brief Gets the unit's active pet as a unit.
 *
 * @return The active pet, or NULL if none exists.
 */
Unit* Unit::_GetPet() const
{
    return GetPet();
}

/**
 * @brief Gets the unit's mini pet as a unit.
 *
 * @return The mini pet, or NULL if none exists.
 */
Unit* Unit::_GetMiniPet() const
{
    return GetMiniPet();
}

/**
 * @brief Resolves a pet by GUID on the current map.
 *
 * @param guid The pet GUID.
 * @return The pet, or NULL if not found.
 */
Unit* Unit::_GetPet(ObjectGuid guid) const
{
    return GetMap()->GetPet(guid);
}

void Unit::RemoveMiniPet()
{
    if (Pet* pet = GetMiniPet())
    {
        pet->Unsummon(PET_SAVE_AS_DELETED, this);
    }
    else
    {
        SetCritterGuid(ObjectGuid());
    }
}

Pet* Unit::GetMiniPet() const
{
    if (!GetCritterGuid())
    {
        return NULL;
    }

    return GetMap()->GetPet(GetCritterGuid());
}

/**
 * @brief Gets the currently charmed unit, if any.
 *
 * @return The charmed unit, or NULL if none exists.
 */
Unit* Unit::GetCharm() const
{
    if (ObjectGuid charm_guid = GetCharmGuid())
    {
        if (Unit* pet = ObjectLookup::GetUnit(*this, charm_guid))
        {
            return pet;
        }

        sLog.outError("Unit::GetCharm: Charmed %s not exist.", charm_guid.GetString().c_str());
        const_cast<Unit*>(this)->SetCharm(NULL);
    }

    return NULL;
}

/**
 * @brief Removes charm and possession auras from the current charm target.
 */
void Unit::Uncharm()
{
    if (Unit* charm = GetCharm())
    {
        charm->RemoveSpellsCausingAura(SPELL_AURA_MOD_CHARM);
        charm->RemoveSpellsCausingAura(SPELL_AURA_MOD_POSSESS);
        charm->RemoveSpellsCausingAura(SPELL_AURA_MOD_POSSESS_PET);

        // TODO:: find a way to get rid of this bad hack to remove Raise ally aura
        if (Creature* charmCreature = charm->ToCreature())
        {
            uint32 createdBySpellId = charm->GetUInt32Value(UNIT_CREATED_BY_SPELL);
            if (charmCreature->IsTemporarySummon() && createdBySpellId)
            {
                RemoveAurasDueToSpell(createdBySpellId);
            }
        }
    }
}

/**
 * @brief Sets the unit's pet GUID.
 *
 * @param pet The pet to assign, or NULL to clear it.
 */
void Unit::SetPet(Pet* pet)
{
    SetPetGuid(pet ? pet->GetObjectGuid() : ObjectGuid());

    if (pet && GetTypeId() == TYPEID_PLAYER)
    {
        SendPetGUIDs();
    }
}

/**
 * @brief Tells a player's own client the guid of its pet, when it has one.
 */
void Unit::SendPetGUIDs()
{
    if (!GetPetGuid())
    {
        return;
    }

    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        CurrentPetFact fact;
        fact.pet = GetPetGuid();
        ReportClientFact(client->currentPet, fact);
    }
}

/**
 * @brief Removes the unit's pet, saving it in a mode.
 *
 * @param mode How the pet is saved.
 */
void Unit::RemovePet(PetSaveMode mode)
{
    if (Pet* pet = GetPet())
    {
        pet->Unsummon(mode, this);
    }
}

/**
 * @brief Tells a player's own client that its pet action bar is cleared.
 */
void Unit::RemovePetActionBar()
{
    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        ReportClientFact(client->petActionBarCleared, PetActionBarClearedFact());
    }
}

/**
 * @brief Tells a player's own client the action bar of the unit it possesses.
 */
void Unit::PossessSpellInitialize()
{
    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        PossessBarFact fact;
        fact.charm = GetCharm();
        ReportClientFact(client->possessBar, fact);
    }
}

/**
 * @brief Sets a player's camera to another unit's view.
 *
 * @param target The unit whose view the camera takes.
 */
void Unit::SetCameraView(Unit* target)
{
    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        ViewFromFact fact;
        fact.target = target;
        ReportClientFact(client->viewFrom, fact);
    }
}

/**
 * @brief Sets a player's camera back to its own view.
 */
void Unit::ResetCameraView()
{
    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        ReportClientFact(client->viewReset, ViewResetFact());
    }
}

/**
 * @brief The item this unit holds under a guid, which the player's item callbacks find.
 *
 * @param guid The item's guid.
 * @return The item, or NULL for none and for a unit that is not a player.
 */
Item* Unit::GetItemByGuid(ObjectGuid guid) const
{
    return m_itemCallbacks ? m_itemCallbacks->findItem(guid) : NULL;
}

/**
 * @brief Asks a player's group whether another player sees it through its stealth.
 *
 * @param p The player that looks at this unit.
 * @return The group's answer; false for a unit that is not a player.
 */
bool Unit::IsGroupVisibleFor(Player const* p) const
{
    if (GroupCallbacks const* group = m_groupCallbacks)
    {
        MANGOS_ASSERT(group->groupVisibleFor);
        return group->groupVisibleFor(p);
    }
    return false;
}

/**
 * @brief Asks a player's group for a random other member near it.
 *
 * @param radius The distance the member stands within.
 * @return The group's answer; NULL for a unit that is not a player.
 */
Player* Unit::GetNextRandomRaidMember(float radius)
{
    if (GroupCallbacks const* group = m_groupCallbacks)
    {
        MANGOS_ASSERT(group->nextRandomRaidMember);
        return group->nextRandomRaidMember(radius);
    }
    return NULL;
}

/**
 * @brief Sets the unit's charm GUID.
 *
 * @param pet The charmed unit to assign, or NULL to clear it.
 */
void Unit::SetCharm(Unit* pet)
{
    SetCharmGuid(pet ? pet->GetObjectGuid() : ObjectGuid());
}

/**
 * @brief Registers a guardian pet under the unit.
 *
 * @param pet The guardian pet to add.
 */
void Unit::AddGuardian(Pet* pet)
{
    m_guardianPets.insert(pet->GetObjectGuid());
}

/**
 * @brief Unregisters a guardian pet from the unit.
 *
 * @param pet The guardian pet to remove.
 */
void Unit::RemoveGuardian(Pet* pet)
{
    m_guardianPets.erase(pet->GetObjectGuid());
}

/**
 * @brief Unsummons and unregisters all guardian pets.
 */
void Unit::RemoveGuardians()
{
    while (!m_guardianPets.empty())
    {
        ObjectGuid guid = *m_guardianPets.begin();

        if (Pet* pet = GetMap()->GetPet(guid))
        {
            pet->Unsummon(PET_SAVE_AS_DELETED, this);  // can remove pet guid from m_guardianPets
        }

        m_guardianPets.erase(guid);
    }
}

/**
 * @brief Finds a guardian pet with a given creature entry.
 *
 * @param entry The creature entry to search for.
 * @return The matching guardian pet, or NULL if none exists.
 */
Pet* Unit::FindGuardianWithEntry(uint32 entry)
{
    for (GuidSet::const_iterator itr = m_guardianPets.begin(); itr != m_guardianPets.end(); ++itr)
        if (Pet* pet = GetMap()->GetPet(*itr))
            if (pet->GetEntry() == entry)
            {
                return pet;
            }

    return NULL;
}

Pet* Unit::GetProtectorPet()
{
    for (GuidSet::const_iterator itr = m_guardianPets.begin(); itr != m_guardianPets.end(); ++itr)
        if (Pet* pet = GetMap()->GetPet(*itr))
            if (pet->getPetType() == PROTECTOR_PET)
            {
                return pet;
            }

    return NULL;
}

/**
 * @brief Gets a totem unit from a totem slot.
 *
 * @param slot The totem slot.
 * @return The totem unit, or NULL if none exists.
 */
Unit* Unit::_GetTotem(TotemSlot slot) const
{
    return GetTotem(slot);
}

/**
 * @brief Gets a totem from a totem slot.
 *
 * @param slot The totem slot.
 * @return The totem, or NULL if none exists.
 */
Totem* Unit::GetTotem(TotemSlot slot) const
{
    if (slot >= MAX_TOTEM_SLOT || !IsInWorld() || !m_TotemSlot[slot])
    {
        return NULL;
    }

    Creature* totem = GetMap()->GetCreature(m_TotemSlot[slot]);
    return totem && totem->IsTotem() ? (Totem*)totem : NULL;
}

/**
 * @brief Checks whether all totem slots are occupied.
 *
 * @return True if all totem slots are used; otherwise, false.
 */
bool Unit::IsAllTotemSlotsUsed() const
{
    for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
        if (!m_TotemSlot[i])
        {
            return false;
        }
    return true;
}

/**
 * @brief Registers a totem in a specific slot.
 *
 * @param slot The totem slot.
 * @param totem The totem to register.
 */
void Unit::_AddTotem(TotemSlot slot, Totem* totem)
{
    m_TotemSlot[slot] = totem->GetObjectGuid();
}

/**
 * @brief Removes a totem from its registered slot.
 *
 * @param totem The totem to unregister.
 */
void Unit::_RemoveTotem(Totem* totem)
{
    for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
    {
        if (m_TotemSlot[i] == totem->GetObjectGuid())
        {
            m_TotemSlot[i].Clear();
            break;
        }
    }
}

/**
 * @brief Unsummons all active totems owned by the unit.
 */
void Unit::UnsummonAllTotems()
{
    for (int i = 0; i < MAX_TOTEM_SLOT; ++i)
        if (Totem* totem = GetTotem(TotemSlot(i)))
        {
            totem->UnSummon();
        }
}

/**
 * @brief Redirects a spell target to an active spell magnet if applicable.
 *
 * @param victim The original victim.
 * @param spell The spell being cast.
 * @param eff The effect index being validated.
 * @return The redirected magnet target, or the original victim.
 */
Unit* Unit::SelectMagnetTarget(Unit* victim, Spell* spell, SpellEffectIndex eff)
{
    if (!victim)
    {
        return NULL;
    }

    // Magic case
    if (spell && (spell->m_spellInfo->GetDmgClass() == SPELL_DAMAGE_CLASS_NONE || spell->m_spellInfo->GetDmgClass() == SPELL_DAMAGE_CLASS_MAGIC))
    {
        Unit::AuraList const& magnetAuras = victim->GetAurasByType(SPELL_AURA_SPELL_MAGNET);
        for (Unit::AuraList::const_iterator itr = magnetAuras.begin(); itr != magnetAuras.end(); ++itr)
        {
            if (Unit* magnet = (*itr)->GetCaster())
            {
                if (magnet->IsAlive() && HasLineOfSight(*magnet, *this) && spell->CheckTarget(magnet, eff))
                {
                    return magnet;
                }
            }
        }
    }
    // Melee && ranged case
    else
    {
        AuraList const& hitTriggerAuras = victim->GetAurasByType(SPELL_AURA_ADD_CASTER_HIT_TRIGGER);
        for (AuraList::const_iterator i = hitTriggerAuras.begin(); i != hitTriggerAuras.end(); ++i)
        {
            if (Unit* magnet = (*i)->GetCaster())
            {
                if (magnet->IsAlive() && HasLineOfSight(*magnet, *this) && (!spell || spell->CheckTarget(magnet, eff)))
                {
                    if (roll_chance_i((*i)->GetModifier()->m_amount))
                    {
                        return magnet;
                    }
                }
            }
        }
    }

    return victim;
}

/**
 * @brief Sends an energize log packet to nearby clients.
 *
 * @param pVictim The energized unit.
 * @param SpellID The spell identifier.
 * @param Damage The gained power amount.
 * @param powertype The power type being restored.
 */
void Unit::SendEnergizeSpellLog(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype)
{
    WorldPacket data(SMSG_SPELLENERGIZELOG, (8 + 8 + 4 + 4 + 4 + 1));
    data << pVictim->GetPackGUID();
    data << GetPackGUID();
    data << uint32(SpellID);
    data << uint32(powertype);
    data << uint32(Damage);
    SendMessageToSet(&data, true);
}

/**
 * @brief Applies power gain from a spell and sends the energize log.
 *
 * @param pVictim The energized unit.
 * @param SpellID The spell identifier.
 * @param Damage The gained power amount.
 * @param powertype The power type being restored.
 */
void Unit::EnergizeBySpell(Unit* pVictim, uint32 SpellID, uint32 Damage, Powers powertype)
{
    SendEnergizeSpellLog(pVictim, SpellID, Damage, powertype);
    // needs to be called after sending spell log
    pVictim->ModifyPower(powertype, Damage);
}

/**
 * @brief Mounts the unit using a display id.
 *
 * @param mount The mount display identifier.
 * @param spellId The mounting spell identifier.
 * @param canFly True when the mount aura's resolved MountCapabilityEntry can fly HERE; the pet
 *               branch below is the only thing that reads it (design 2026-09-22 §4).
 */
void Unit::Mount(uint32 mount, uint32 spellId, bool canFly)
{
    if (!mount)
    {
        return;
    }

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_MOUNTING);

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, mount);

    SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT);

    if (GetTypeId() == TYPEID_PLAYER)
    {
        // Called by Taxi system / GM command
        if (!spellId)
        {
            UnsummonPetTemporaryIfAny();
        }
        // Called by mount aura
        else if (sSpellStore.LookupEntry(spellId))
        {
            // THE 4.2.0 RULE (design 2026-09-22 §4; Wowpedia 2011-06-06, Petopia 4.2, PTR-tested
            // for hunter, warlock and death knight): a permanent pet STAYS OUT AND FOLLOWS on a
            // ground mount, and despawns only on lift-off with a flying one, returning on
            // dismount.
            //
            // The test used to be `IsSpellHaveAura(spellInfo, SPELL_AURA_MOD_FLIGHT_SPEED_MOUNTED)`
            // on the MOUNT SPELL. That is dead on 4.3.4 and had been since 4.0: the mount spell
            // carries nothing but SPELL_AURA_MOUNTED (verified over SpellEffect.dbc -- 55164,
            // 30174, 32235 each have exactly one effect), because the speed moved to the
            // capability's SpeedModSpell, which is where aura 207 lives now (86459/86460/86461).
            // So the flying arm never fired, every mount fell into the arm below, and its
            // condition unsummoned any controlled non-temporary pet -- i.e. the GROUND half was
            // the broken one, the opposite of what it looked like.
            //
            // The answer now comes from the capability the aura resolved, which is already gated
            // on the zone, the map, the riding skill and the licence aura: a flying mount used
            // where flight is forbidden resolves to a GROUND capability (mount type 248 on map 1
            // without spell 90267 returns capability 227, Flags 0x1d) and the pet rightly stays.
            // The predicate is `Flags & 0x2`, the same bit Unit::GetMountCapability itself treats
            // as the flying half of the land pair; testing the capability's SpeedModSpell for
            // aura 207 instead would agree on all 38 MountCapability rows.
            //
            // AND THE FLYING ARM DOES NOT DESPAWN ANYTHING HERE (live test 2026-09-22, C4/T7):
            // retail's trigger is LIFT-OFF, not mounting, and #116 hung it on the aura's apply,
            // so the pet vanished the instant a flying mount was summoned. The despawn moved to
            // MovementHandlers::HandleMoverRelocation, where the first movement word carrying
            // MOVEFLAG_FLYING under a mount is what fires it. `canFly` is still resolved and
            // handed down because it is the one thing that says whether this mount can ever
            // lift off at all, and it is what the harness reads to prove the rule; the pet
            // itself is now kept on BOTH kinds of mount, exactly as a ground one always should
            // have kept it.
            DEBUG_LOG("Unit::Mount: %s mounted with spell %u, capability canFly=%u -- the pet stays until lift-off",
                      GetGuidStr().c_str(), spellId, canFly ? 1 : 0);

            if (Pet* pet = GetPet())
            {
                // A permanent pet is kept, with its actions disabled while its owner is
                // mounted -- which is what the client shows, a greyed-out pet bar, and what
                // Unit::Unmount restores from the CharmInfo. A TEMPORARY summon keeps exactly
                // the gate it had: PetUnsummonAtMount, and the arena with it.
                if (pet->isControlled() && (pet->isTemporarySummoned() || InArena())
                    && sGameConfig.getConfig(CONFIG_BOOL_PET_UNSUMMON_AT_MOUNT))
                {
                    UnsummonPetTemporaryIfAny();
                }
                else
                {
                    pet->SetModeFlags(PET_MODE_DISABLE_ACTIONS);
                }
            }

            float height = GetCollisionHeight(true);
            if (height)
            {
                SendCollisionHeightUpdate(height);
            }
        }
    }
}

/**
 * @brief Unmounts the unit and optionally broadcasts a dismount packet.
 *
 * @param from_aura True when the unmount is caused by aura removal.
 */
void Unit::Unmount(bool from_aura)
{
    if (!IsMounted())
    {
        return;
    }

    RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_NOT_MOUNTED);

    SetUInt32Value(UNIT_FIELD_MOUNTDISPLAYID, 0);
    RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_MOUNT);

    // Called NOT by Taxi system / GM command
    if (from_aura)
    {
        WorldPacket data(SMSG_DISMOUNT, 8);
        data << GetPackGUID();
        SendMessageToSet(&data, true);
    }

    // only resummon old pet if the player is already added to a map
    // this prevents adding a pet to a not created map which would otherwise cause a crash
    // (it could probably happen when logging in after a previous crash)
    if (GetTypeId() == TYPEID_PLAYER)
    {
        if (Pet* pet = GetPet())
        {
            // Get reaction state and display appropriately
            if (CharmInfo* charmInfo = pet->GetCharmInfo())
            {
                pet->SetModeFlags(PetModeFlags(charmInfo->GetReactState() | charmInfo->GetCommandState() * 0x100));
            }
        }
        else
        {
            ResummonPetTemporaryUnSummonedIfAny();
        }

        float height = GetCollisionHeight(false);
        if (height)
        {
            SendCollisionHeightUpdate(height);
        }
    }
}

/**
 * @brief Checks whether a position is close enough to a waypoint destination.
 *
 * @param currentPositionX The current X coordinate.
 * @param currentPositionY The current Y coordinate.
 * @param currentPositionZ The current Z coordinate.
 * @param destinationPostionX The destination X coordinate.
 * @param destinationPostionY The destination Y coordinate.
 * @param destinationPostionZ The destination Z coordinate.
 * @param distanceX The allowed X distance.
 * @param distanceY The allowed Y distance.
 * @param distanceZ The allowed Z distance.
 * @return True if the waypoint is considered reached; otherwise, false.
 */

MountCapabilityEntry const* Unit::GetMountCapability(uint32 mountType) const
{
    if (!mountType)
    {
        return NULL;
    }

    MountTypeEntry const* mountTypeEntry = sMountTypeStore.LookupEntry(mountType);
    if (!mountTypeEntry)
    {
        return NULL;
    }

    uint32 zoneId, areaId;
    GetTerrain()->GetZoneAndAreaId(zoneId, areaId, Where().X(), Where().Y(), Where().Z());
    uint32 ridingSkill = 5000;
    if (Player const* plr = ToPlayer())
    {
        ridingSkill = plr->GetSkillValue(SKILL_RIDING);
    }

    MovementInfo const status = WireStatus();
    for (uint32 i = MAX_MOUNT_CAPABILITIES; i > 0; --i)
    {
        MountCapabilityEntry const* mountCapability = sMountCapabilityStore.LookupEntry(mountTypeEntry->Capability[i - 1]);
        if (!mountCapability)
        {
            continue;
        }

        if (ridingSkill < mountCapability->RequiredRidingSkill)
        {
            continue;
        }

        if (status.HasMovementFlag2(MOVEFLAG2_FULLSPEEDPITCHING))
        {
            if (!(mountCapability->Flags & MOUNT_FLAG_CAN_PITCH))
            {
                continue;
            }
        }
        else if (status.HasMovementFlag(MOVEFLAG_SWIMMING))
        {
            if (!(mountCapability->Flags & MOUNT_FLAG_CAN_SWIM))
            {
                continue;
            }
        }
        else if (!(mountCapability->Flags & 0x1))   // unknown flags, checked in 4.2.2 14545 client
        {
            if (!(mountCapability->Flags & 0x2))
            {
                continue;
            }
        }

        if (mountCapability->RequiredMap != -1 && int32(GetMapId()) != mountCapability->RequiredMap)
        {
            continue;
        }

        if (mountCapability->RequiredArea && (mountCapability->RequiredArea != zoneId && mountCapability->RequiredArea != areaId))
        {
            continue;
        }

        if (mountCapability->RequiredAura && !HasAura(mountCapability->RequiredAura))
        {
            continue;
        }

        if (mountCapability->RequiredSpell && (GetTypeId() != TYPEID_PLAYER || !HasSpell(mountCapability->RequiredSpell)))
        {
            continue;
        }

        return mountCapability;
    }

    return NULL;
}

void Unit::PlayOneShotAnimKit(uint32 id)
{
    WorldPacket data(SMSG_PLAY_ONE_SHOT_ANIM_KIT, 7+2);
    data << GetPackGUID();
    data << uint16(id);
    SendMessageToSet(&data, true);
}

/**
 * @brief Adds or removes health while clamping to valid limits.
 *
 * @param dVal The signed health delta.
 * @return The effective health change.
 */
int32 Unit::ModifyHealth(int32 dVal)
{
    if (dVal == 0)
    {
        return 0;
    }

    int32 curHealth = (int32)GetHealth();

    int32 val = dVal + curHealth;
    if (val <= 0)
    {
        SetHealth(0);
        return -curHealth;
    }

    int32 maxHealth = (int32)GetMaxHealth();

    int32 gain;
    if (val < maxHealth)
    {
        SetHealth(val);
        gain = val - curHealth;
    }
    else
    {
        SetHealth(maxHealth);
        gain = maxHealth - curHealth;
    }

    return gain;
}

/**
 * @brief Adds or removes a power type while clamping to valid limits.
 *
 * @param power The power type to modify.
 * @param dVal The signed power delta.
 * @return The effective power change.
 */
int32 Unit::ModifyPower(Powers power, int32 dVal)
{
    if (dVal == 0)
    {
        return 0;
    }

    int32 curPower = (int32)GetPower(power);

    int32 val = dVal + curPower;
    if (val <= 0)
    {
        SetPower(power, 0);
        return -curPower;
    }

    int32 maxPower = (int32)GetMaxPower(power);

    int32 gain;
    if (val < maxPower)
    {
        SetPower(power, val);
        gain = val - curPower;
    }
    else
    {
        SetPower(power, maxPower);
        gain = maxPower - curPower;
    }

    return gain;
}

/**
 * @brief Changes the unit death state and applies death or revival side effects.
 *
 * @param s The new death state.
 */
void Unit::SetDeathState(DeathState s)
{
    if (s != ALIVE && s != JUST_ALIVED)
    {
        CombatStop();
        DeleteThreatList();
        ClearComboPointHolders();                           // any combo points pointed to unit lost at it death

        if (IsNonMeleeSpellCasted(false))
        {
            InterruptNonMeleeSpells(false);
        }
    }

    if (s == JUST_DIED)
    {
        RemoveAllAurasOnDeath();
        RemoveGuardians();
        RemoveMiniPet();
        UnsummonAllTotems();

        StopMoving();
        i_motionMaster->Die();

        // Unsummon vehicle accessories
        if (IsVehicle())
        {
            m_vehicleInfo->RemoveAccessoriesFromMap();
        }

        // Unboard from transport
        if (GetTransportInfo() && ((Unit*)GetTransportInfo()->GetTransport())->IsVehicle())
        {
            ((Unit*)GetTransportInfo()->GetTransport())->RemoveSpellsCausingAura(SPELL_AURA_CONTROL_VEHICLE, GetObjectGuid());
        }

        ModifyAuraState(AURA_STATE_HEALTHLESS_20_PERCENT, false);
        ModifyAuraState(AURA_STATE_HEALTHLESS_35_PERCENT, false);
        // remove aurastates allowing special moves
        ClearAllReactives();
        ClearDiminishings();
    }
    else if (s == JUST_ALIVED)
    {
        RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_SKINNABLE);  // clear skinnable for creature and player (at battleground)
    }

    if (m_deathState != ALIVE && s == ALIVE)
    {
        //_ApplyAllAuraMods();
    }

    if (s == JUST_ALIVED || s == ALIVE)
    {
        i_motionMaster->Uninhibit(Motion::Inhibition::Dead, Motion::kDeathSource);
    }

    m_deathState = s;
}

//======================================================================
//======================================================================
//======================================================================

int32 Unit::CalculateSpellDamage(Unit const* target, SpellEntry const* spellProto, SpellEffectIndex effect_index, int32 const* effBasePoints)
{
    SpellEffectEntry const* spellEffect = spellProto->GetSpellEffect(effect_index);
    if (!spellEffect)
    {
        return 0;
    }

    switch (spellEffect->EffectAura)
    {
        case SPELL_AURA_MOUNTED:
            if (MountCapabilityEntry const* mountCapability = GetMountCapability(uint32(spellEffect->EffectMiscValue_1)))
            {
                return int32(mountCapability->ID);
            }
            break;
        default:
            break;
    }

    bool unitPlayer = GetTypeId() == TYPEID_PLAYER;
    uint32 level = getLevel();

    // calculate basepoints dependent on mastery
    if (unitPlayer && spellProto->HasAttribute(SPELL_ATTR_EX8_MASTERY) && !spellProto->CalculateSimpleValue(effect_index))
    {
        if (int32 masteryCoef = GetMasteryCoefficient(spellProto))
        {
            return int32(GetFloatValue(PLAYER_MASTERY) * masteryCoef / 100.0f);
        }
    }

    // calculate basepoints for armor specialization spells
    if (unitPlayer && spellProto->HasAttribute(SPELL_ATTR_EX8_ARMOR_SPECIALIZATION))
    {
        // check spells not valid for current talent tree or insufficient equipped items
        if (!FitArmorSpecializationRules(spellProto))
        {
            return 0;
        }
    }

    uint8 comboPoints = unitPlayer ? GetComboPoints() : 0;

    int32 basePoints = 0;
    uint32 spellLevel = 0;
    float comboDamage = 0.0f;

    SpellScalingEntry const* scalingEntry = spellProto->GetSpellScaling();
    GtSpellScalingEntry const* gtScalingEntry = NULL;
    if (scalingEntry && scalingEntry->IsScalableEffect(effect_index))
    {
        if (target && IsAuraApplyEffect(spellProto, effect_index) && IsPositiveEffect(spellProto, effect_index))
        {
            level = target->getLevel();
        }

        uint32 gtSpellScalingId = level - 1;
        if (scalingEntry->Class == -1)
        {
            gtSpellScalingId += (MAX_CLASSES - 1) * GT_MAX_LEVEL;
        }
        else
        {
            gtSpellScalingId += (scalingEntry->Class - 1) * GT_MAX_LEVEL;
        }

        gtScalingEntry = sGtSpellScalingStore.LookupEntry(gtSpellScalingId);
    }

    if (gtScalingEntry)
    {
        float Scale = gtScalingEntry->value;
        if (uint32(scalingEntry->CastTimeMax) > 0 && uint32(scalingEntry->CastTimeMaxLevel) > level)
        {
            Scale *= float(scalingEntry->CastTimeMin + float(level - 1) * (scalingEntry->CastTimeMax - scalingEntry->CastTimeMin) / (scalingEntry->CastTimeMaxLevel - 1)) / float(scalingEntry->CastTimeMax);
        }
        if (uint32(scalingEntry->NerfMaxLevel) > level)
        {
            Scale *= (1.0f - scalingEntry->NerfFactor) * (level - 1) / (scalingEntry->NerfMaxLevel - 1) + scalingEntry->NerfFactor;
        }

        basePoints = int32(scalingEntry->Coefficient[effect_index] * Scale);
        int32 randomPoints = int32(scalingEntry->Coefficient[effect_index] * Scale * scalingEntry->Variance[effect_index]);
        basePoints += irand(-randomPoints, randomPoints) / 2;
        comboDamage = uint32(scalingEntry->ComboPointsCoefficient[effect_index] * Scale);
    }
    else
    {
        spellLevel = spellProto->GetSpellLevel(); // reaches the DBC store
        uint32 maxLevel = spellProto->GetMaxLevel();
        uint32 baseLevel = spellProto->GetBaseLevel();

        Combat::SpellLegacyScaling const legacy = Combat::SpellLegacyScalingPoints(
                level, spellLevel, maxLevel, baseLevel, spellEffect->EffectRealPointsPerLevel,
                effBasePoints != NULL, effBasePoints ? *effBasePoints : 0,
                spellEffect->EffectBasePoints, spellEffect->EffectDieSides,
                spellEffect->EffectPointsPerResource);
        level = legacy.level;
        basePoints = legacy.basePoints;
        int32 randomPoints = legacy.randomPoints;
        comboDamage = legacy.comboDamage;

        switch (randomPoints)
        {
            case 0:                                             // not used
            case 1: basePoints += 1; break;                     // range 1..1
            default:
            {
                // range can have positive (1..rand) and negative (rand..1) values, so order its for irand
                int32 randvalue = (randomPoints >= 1)
                                  ? irand(1, randomPoints)
                                  : irand(randomPoints, 1);

                basePoints += randvalue;
                break;
            }
        }
    }

    int32 value = basePoints;

    // random damage
    if (comboDamage != 0 && unitPlayer && target && (target->GetObjectGuid() == GetComboTargetGuid() || spellProto->HasAttribute(SPELL_ATTR_EX8_IGNORE_TARGET_FOR_COMBO_POINTS)))
    {
        value += (int32)(comboDamage * comboPoints);
    }

    if (SpellModMgr* modOwner = GetSpellMods())
    {
        modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_ALL_EFFECTS, value);

        switch (effect_index)
        {
            case EFFECT_INDEX_0:
                modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_EFFECT1, value);
                break;
            case EFFECT_INDEX_1:
                modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_EFFECT2, value);
                break;
            case EFFECT_INDEX_2:
                modOwner->ApplySpellMod(spellProto->ID, SPELLMOD_EFFECT3, value);
                break;
        }
    }

    if (!gtScalingEntry && spellProto->Attributes & SPELL_ATTR_LEVEL_DAMAGE_CALCULATION && spellLevel &&
            spellEffect->Effect != SPELL_EFFECT_WEAPON_PERCENT_DAMAGE &&
            spellEffect->Effect != SPELL_EFFECT_KNOCK_BACK &&
            (spellEffect->Effect != SPELL_EFFECT_APPLY_AURA || spellEffect->EffectAura != SPELL_AURA_MOD_DECREASE_SPEED))
    {
        value = int32(value * 0.25f * exp(level * (70 - spellLevel) / 1000.0f));
    }

    return value;
}

/**
 * @brief Checks whether the unit is visible to a player without stealth detection.
 *
 * @param u The observing player.
 * @param viewPoint The viewpoint used for visibility checks.
 * @param inVisibleList True when evaluating an existing visible-list entry.
 * @return True if the unit is visible; otherwise, false.
 */
bool Unit::IsVisibleForInState(Player const* u, WorldObject const* viewPoint, bool inVisibleList) const
{
    return IsVisibleForOrDetect(u, viewPoint, false, inVisibleList, false);
}

/// returns true if creature can't be seen by alive units
bool Unit::IsInvisibleForAlive() const
{
    if (m_AuraFlags & UNIT_AURAFLAG_ALIVE_INVISIBLE)
    {
        return true;
    }
    // TODO: maybe spiritservices also have just an aura
    return IsSpiritService();
}

/**
 * @brief Gets the creature type of the unit's shapeshift form, humanoid without one.
 *
 * @return The creature type identifier.
 */
uint32 Unit::GetCreatureType() const
{
    SpellShapeshiftFormEntry const* ssEntry = sSpellShapeshiftFormStore.LookupEntry(GetShapeshiftForm());
    if (ssEntry && ssEntry->CreatureType > 0)
    {
        return ssEntry->CreatureType;
    }
    else
    {
        return CREATURE_TYPE_HUMANOID;
    }
}

/**
 * @brief Sets the unit level and updates group state when needed.
 *
 * @param lvl The new level.
 */
void Unit::SetLevel(uint32 lvl)
{
    SetUInt32Value(UNIT_FIELD_LEVEL, lvl);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        ReportGroupFact(InstalledGroupCallbacks(m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_LEVEL});
    }
}

/**
 * @brief Sets current health while clamping to maximum health.
 *
 * @param val The requested health value.
 */
void Unit::SetHealth(uint32 val)
{
    uint32 maxHealth = GetMaxHealth();
    if (maxHealth < val)
    {
        val = maxHealth;
    }

    SetUInt32Value(UNIT_FIELD_HEALTH, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        ReportGroupFact(InstalledGroupCallbacks(m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_CUR_HP});
    }
    else if (Pet* pet = ToPet(this))
    {
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER))
            {
                ReportGroupFact(InstalledGroupCallbacks(owner->m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_PET_CUR_HP});
            }
        }
    }
}

/**
 * @brief Sets maximum health and clamps current health if necessary.
 *
 * @param val The new maximum health.
 */
void Unit::SetMaxHealth(uint32 val)
{
    uint32 health = GetHealth();
    SetUInt32Value(UNIT_FIELD_MAXHEALTH, val);

    // group update
    if (GetTypeId() == TYPEID_PLAYER)
    {
        ReportGroupFact(InstalledGroupCallbacks(m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_MAX_HP});
    }
    else if (Pet* pet = ToPet(this))
    {
        if (pet->isControlled())
        {
            Unit* owner = GetOwner();
            if (owner && (owner->GetTypeId() == TYPEID_PLAYER))
            {
                ReportGroupFact(InstalledGroupCallbacks(owner->m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_PET_MAX_HP});
            }
        }
    }

    if (val < health)
    {
        SetHealth(val);
    }
}

/**
 * @brief Sets current health as a percentage of maximum health.
 *
 * @param percent The percentage to apply.
 */
void Unit::SetHealthPercent(float percent)
{
    uint32 newHealth = GetMaxHealth() * percent / 100.0f;
    SetHealth(newHealth);
}

/**
 * @brief Adds the unit to the world and initializes world-dependent helpers.
 */
void Unit::AddToWorld()
{
    Object::AddToWorld();
    ScheduleAINotify(0);
}

/**
 * @brief Removes the unit from the world and cleans up world-bound state.
 */
void Unit::RemoveFromWorld()
{
    // cleanup
    if (IsInWorld())
    {
        // A unit a client was moving leaves that client's set, with no packet: the
        // charm code sends its own when it runs, and this is the net under it for any
        // creature, pet or summon. A player keeps its membership across a far teleport
        // (the worldport's grant is idempotent) and is revoked by LogoutPlayer. A unit
        // changing map goes through here too (a minion drawn onto or off a vessel's
        // deck crosses by Map::Remove the same as any other map change), so it is
        // revoked the same way, without a packet; nothing today possesses a minion
        // across a deck edge, so that gap is named here rather than closed.
        if (m_moverSession && GetTypeId() != TYPEID_PLAYER)
        {
            m_moverSession->RevokeMover(this, GameTime::GetGameTimeMS());
        }

        Uncharm();
        RemoveNotOwnTrackedTargetAuras();
        RemoveGuardians();
        RemoveMiniPet();
        UnsummonAllTotems();
        RemoveAllGameObjects();
        RemoveAllDynObjects();
        CleanupDeletedAuras();
        GetViewPoint().Event_RemovedFromWorld();
    }

    Object::RemoveFromWorld();
}

/**
 * @brief Performs final cleanup before deleting the unit object.
 */
void Unit::CleanupsBeforeDelete()
{
    if (m_uint32Values)                                     // only for fully created object
    {
        InterruptNonMeleeSpells(true);
        m_Events.KillAllEvents(false);                      // non-delatable (currently casted spells) will not deleted now but it will deleted at call in Map::RemoveAllObjectsInRemoveList
        CombatStop();
        ClearComboPointHolders();
        DeleteThreatList();
        if (GetTypeId() == TYPEID_PLAYER)
        {
            GetHostileRefManager().setOnlineOfflineState(false);
        }
        else
        {
            GetHostileRefManager().deleteReferences();
        }
        RemoveAllAuras(AURA_REMOVE_BY_DELETE);
    }
    WorldObject::CleanupsBeforeDelete();
}

/**
 * @brief Ensures charm information exists for the unit.
 *
 * @param charm The charmed or possessed unit.
 * @return The initialized charm info instance.
 */
CharmInfo* Unit::InitCharmInfo(Unit* charm)
{
    if (!m_charmInfo)
    {
        m_charmInfo = new CharmInfo(charm);
    }
    return m_charmInfo;
}

CharmInfo::CharmInfo(Unit* unit)
    : m_unit(unit), m_CommandState(COMMAND_FOLLOW), m_reactState(REACT_PASSIVE), m_petnumber(0)
{
    for (int i = 0; i < CREATURE_MAX_SPELLS; ++i)
    {
        m_charmspells[i].SetActionAndType(0, ACT_DISABLED);
    }
}

/**
 * @brief Initializes the default pet action bar layout.
 */
void CharmInfo::InitPetActionBar()
{
    // the first 3 SpellOrActions are attack, follow and stay
    for (uint32 i = 0; i < ACTION_BAR_INDEX_PET_SPELL_START - ACTION_BAR_INDEX_START; ++i)
    {
        SetActionBar(ACTION_BAR_INDEX_START + i, COMMAND_ATTACK - i, ACT_COMMAND);
    }

    // middle 4 SpellOrActions are spells/special attacks/abilities
    for (uint32 i = 0; i < ACTION_BAR_INDEX_PET_SPELL_END - ACTION_BAR_INDEX_PET_SPELL_START; ++i)
    {
        SetActionBar(ACTION_BAR_INDEX_PET_SPELL_START + i, 0, ACT_DISABLED);
    }

    // last 3 SpellOrActions are reactions
    for (uint32 i = 0; i < ACTION_BAR_INDEX_END - ACTION_BAR_INDEX_PET_SPELL_END; ++i)
    {
        SetActionBar(ACTION_BAR_INDEX_PET_SPELL_END + i, COMMAND_ATTACK - i, ACT_REACTION);
    }
}

/**
 * @brief Initializes an empty passive action bar.
 */
void CharmInfo::InitEmptyActionBar()
{
    for (uint32 x = ACTION_BAR_INDEX_START; x < ACTION_BAR_INDEX_END; ++x)
    {
        SetActionBar(x, 0, ACT_PASSIVE);
    }
}

void CharmInfo::InitVehicleCreateSpells()
{
    InitEmptyActionBar();

    Creature* creature = m_unit->ToCreature();
    if (!creature)                                          // player vehicles don't have spells, keep the action bar empty
    {
        return;
    }

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        if (IsPassiveSpell(creature->m_spells[x]))
        {
            m_unit->CastSpell(m_unit, creature->m_spells[x], true);
        }
        else
        {
            AddSpellToActionBar(creature->m_spells[x], ActiveStates(0x8 + x));
        }
    }
}

/**
 * @brief Initializes spells and action bar entries for possession control.
 */
void CharmInfo::InitPossessCreateSpells()
{
    InitEmptyActionBar();                                   // charm action bar

    Creature* creature = m_unit->ToCreature();
    if (!creature)                                          // possessed players don't have spells, keep the action bar empty
    {
        return;
    }

    SetActionBar(ACTION_BAR_INDEX_START, COMMAND_ATTACK, ACT_COMMAND);

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        if (IsPassiveSpell(creature->m_spells[x]))
        {
            m_unit->CastSpell(m_unit, creature->m_spells[x], true);
        }
        else
        {
            AddSpellToActionBar(creature->m_spells[x], ACT_PASSIVE);
        }
    }
}

/**
 * @brief Initializes spells and action bar entries for charm control.
 */
void CharmInfo::InitCharmCreateSpells()
{
    Creature* creature = m_unit->ToCreature();
    if (!creature)                                          // charmed players don't have spells
    {
        InitEmptyActionBar();
        return;
    }

    InitPetActionBar();

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
    {
        uint32 spellId = creature->m_spells[x];

        if (!spellId)
        {
            m_charmspells[x].SetActionAndType(spellId, ACT_DISABLED);
            continue;
        }

        if (IsPassiveSpell(spellId))
        {
            m_unit->CastSpell(m_unit, spellId, true);
            m_charmspells[x].SetActionAndType(spellId, ACT_PASSIVE);
        }
        else
        {
            m_charmspells[x].SetActionAndType(spellId, ACT_DISABLED);

            ActiveStates newstate;
            bool onlyselfcast = true;
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(spellId);

            for (uint32 i = 0; i < 3 && onlyselfcast; ++i)  // nonexistent spell will not make any problems as onlyselfcast would be false -> break right away
            {
                SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(SpellEffectIndex(i));
                if (!spellEffect)
                {
                    continue;
                }
                if (spellEffect->ImplicitTarget_0 != TARGET_SELF && spellEffect->ImplicitTarget_0 != 0)
                {
                    onlyselfcast = false;
                }
            }

            if (onlyselfcast || !IsPositiveSpell(spellId))  // only self cast and spells versus enemies are autocastable
            {
                newstate = ACT_DISABLED;
            }
            else
            {
                newstate = ACT_PASSIVE;
            }

            AddSpellToActionBar(spellId, newstate);
        }
    }
}

/**
 * @brief Adds or upgrades a spell entry on the pet action bar.
 *
 * @param spell_id The spell identifier to add.
 * @param newstate The desired active state.
 * @return True if the spell was placed on the action bar; otherwise, false.
 */
bool CharmInfo::AddSpellToActionBar(uint32 spell_id, ActiveStates newstate)
{
    uint32 first_id = sSpellMgr.GetFirstSpellInChain(spell_id);

    // new spell rank can be already listed
    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (uint32 action = PetActionBar[i].GetAction())
        {
            if (PetActionBar[i].IsActionBarForSpell() && sSpellMgr.GetFirstSpellInChain(action) == first_id)
            {
                PetActionBar[i].SetAction(spell_id);
                return true;
            }
        }
    }

    // or use empty slot in other case
    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (!PetActionBar[i].GetAction() && PetActionBar[i].IsActionBarForSpell())
        {
            SetActionBar(i, spell_id, newstate == ACT_DECIDE ? ACT_DISABLED : newstate);
            return true;
        }
    }
    return false;
}

/**
 * @brief Removes a spell entry from the pet action bar.
 *
 * @param spell_id The spell identifier to remove.
 * @return True if the spell was found and removed; otherwise, false.
 */
bool CharmInfo::RemoveSpellFromActionBar(uint32 spell_id)
{
    uint32 first_id = sSpellMgr.GetFirstSpellInChain(spell_id);

    for (uint8 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (uint32 action = PetActionBar[i].GetAction())
        {
            if (PetActionBar[i].IsActionBarForSpell() && sSpellMgr.GetFirstSpellInChain(action) == first_id)
            {
                SetActionBar(i, 0, ACT_DISABLED);
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Enables or disables autocast for a creature charm spell.
 *
 * @param spellid The spell identifier.
 * @param apply True to enable autocast; false to disable it.
 */
void CharmInfo::ToggleCreatureAutocast(uint32 spellid, bool apply)
{
    if (IsPassiveSpell(spellid))
    {
        return;
    }

    for (uint32 x = 0; x < CREATURE_MAX_SPELLS; ++x)
        if (spellid == m_charmspells[x].GetAction())
        {
            m_charmspells[x].SetType(apply ? ACT_ENABLED : ACT_DISABLED);
        }
}

/**
 * @brief Sets the stored pet number and optional stat-window field.
 *
 * @param petnumber The pet number to assign.
 * @param statwindow True to expose the number in the unit field.
 */
void CharmInfo::SetPetNumber(uint32 petnumber, bool statwindow)
{
    m_petnumber = petnumber;
    if (statwindow)
    {
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, m_petnumber);
    }
    else
    {
        m_unit->SetUInt32Value(UNIT_FIELD_PETNUMBER, 0);
    }
}

/**
 * @brief Loads a serialized pet action bar layout.
 *
 * @param data The serialized action bar data string.
 */
void CharmInfo::LoadPetActionBar(const std::string& data)
{
    InitPetActionBar();

    Tokens tokens = StrSplit(data, " ");

    if (tokens.size() != (ACTION_BAR_INDEX_END - ACTION_BAR_INDEX_START) * 2)
    {
        return;                                              // non critical, will reset to default
    }

    int index;
    Tokens::iterator iter;
    for (iter = tokens.begin(), index = ACTION_BAR_INDEX_START; index < ACTION_BAR_INDEX_END; ++iter, ++index)
    {
        // use unsigned cast to avoid sign negative format use at long-> ActiveStates (int) conversion
        uint8 type  = (uint8)std::stoul((*iter).c_str());
        ++iter;
        uint32 action = std::stoul((*iter).c_str());

        PetActionBar[index].SetActionAndType(action, ActiveStates(type));

        // check correctness
        if (PetActionBar[index].IsActionBarForSpell() && !sSpellStore.LookupEntry(PetActionBar[index].GetAction()))
        {
            SetActionBar(index, 0, ACT_DISABLED);
        }
    }
}

/**
 * @brief Serializes the action bar into a packet.
 *
 * @param data The packet being populated.
 */
void CharmInfo::BuildActionBar(WorldPacket* data)
{
    for (uint32 i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        *data << uint32(PetActionBar[i].packedData);
    }
}

/**
 * @brief Sets autocast state for a spell already present on the action bar.
 *
 * @param spell_id The spell identifier.
 * @param state True to enable autocast; false to disable it.
 */
void CharmInfo::SetSpellAutocast(uint32 spell_id, bool state)
{
    for (int i = 0; i < MAX_UNIT_ACTION_BAR_INDEX; ++i)
    {
        if (spell_id == PetActionBar[i].GetAction() && PetActionBar[i].IsActionBarForSpell())
        {
            PetActionBar[i].SetType(state ? ACT_ENABLED : ACT_DISABLED);
            break;
        }
    }
}

/**
 * @brief Gets the spell modifiers that apply to this unit's spells.
 *
 * @return The player's own, or a pet's or totem's player owner's; NULL otherwise.
 */
SpellModMgr* Unit::GetSpellMods() const
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        return m_spellMods;
    }
    if (IsPet() || IsTotem())
    {
        Unit* owner = GetOwner();
        if (owner && owner->GetTypeId() == TYPEID_PLAYER)
        {
            return owner->m_spellMods;
        }
    }
    return NULL;
}

///----------Pet responses methods-----------------

/**
 * @brief Sends a pet action feedback packet to the owner.
 *
 * @param msg The feedback opcode payload.
 */
void Unit::SendPetActionFeedback(uint8 msg)
{
    Unit* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    if (ClientCallbacks const* client = owner->GetClientCallbacks())
    {
        PetActionFeedbackFact fact;
        fact.message = msg;
        ReportClientFact(client->petActionFeedback, fact);
    }
}

/**
 * @brief Sends a pet sound reaction packet to the owner.
 *
 * @param pettalk The pet talk identifier.
 */
void Unit::SendPetTalk(uint32 pettalk)
{
    Unit* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    if (ClientCallbacks const* client = owner->GetClientCallbacks())
    {
        PetTalkFact fact;
        fact.pet = GetObjectGuid();
        fact.talk = pettalk;
        ReportClientFact(client->petTalk, fact);
    }
}

/**
 * @brief Sends the default hostile AI reaction packet for a pet.
 */
void Unit::SendPetAIReaction()
{
    Unit* owner = GetOwner();
    if (!owner)
    {
        return;
    }

    if (ClientCallbacks const* client = owner->GetClientCallbacks())
    {
        PetAiReactionFact fact;
        fact.pet = GetObjectGuid();
        fact.reaction = AI_REACTION_HOSTILE;
        ReportClientFact(client->petAiReaction, fact);
    }
}

///----------End of Pet responses methods----------

/**
 * @brief Whether the unit is rooted to the ground (can't move): the kernel's one answer.
 * @return True while any source holds the Rooted inhibition.
 * \see MotionMaster::Inhibited
 */
bool Unit::IsRooted() const
{
    return i_motionMaster->Inhibited(Motion::Inhibition::Rooted);
}

void Unit::StopMoving(bool forceSendStop /*=false*/)
{
    i_motionMaster->ClearMovingLatches();   // the legs the moving mask held (P5-C3)

    // not need send any packets if not in world
    if (!IsInWorld())
    {
        return;
    }

    // Gate on the spline, not on the leg latches (the old *_MOVE states): home legs, effects and raw script
    // splines set none, and skipping them left the spline running.
    if (movespline->Finalized() && !forceSendStop)
    {
        return;
    }

    // A jump or a fall is ballistic: it cannot stop mid-air, so it is left to land and
    // its effect ends there. Only an interrupt (forced) cuts it.
    if (movespline->Airborne() && !forceSendStop)
    {
        return;
    }

    // Take where the spline actually is before stopping there, so the stop packet and
    // the placement agree. The placement itself is written on the next Update.
    CommitSplinePosition();

    Movement::MoveSplineInit init(*this);
    init.Stop();
}

/**
 * @brief Interrupts spline movement and updates the unit position immediately.
 *
 * @param forceSendStop True to force a stop packet even if already stopped.
 */
void Unit::InterruptMoving(bool forceSendStop /*=false*/)
{
    // One stop path: commit the in-flight position and finalize through Stop(), which
    // sends the stop packet and leaves a spline the driver reads as cut, not arrived.
    StopMoving(forceSendStop || !movespline->Finalized());
}

bool Unit::CommitSplinePosition()
{
    if (movespline->Finalized())
    {
        return false;
    }

    Movement::Location loc = movespline->ComputePosition();

    if (IsBoarded())
    {
        // Boarded spline coordinates are seat-local: update the seat pose
        Geometry::Placement deckPose;
        deckPose.EnterFrame(GetTransportInfo()->Seat().CurrentFrame(),
                            Geometry::Vector3(loc.x, loc.y, loc.z), loc.orientation);
        GetTransportInfo()->SetSeatPose(deckPose);
        return true;
    }

    // The placement is written on the next Update. A stop can come from inside a grid
    // visit (an AI reacting to what just walked into view), where a cell switch would
    // break the list being walked; Update is the one place it is safe. Where the unit
    // stands now is kept, so a relocation by anything else in between is not undone.
    m_pendingCommit = Position(loc.x, loc.y, loc.z, loc.orientation);
    m_pendingCommitFrom = Position(Where().X(), Where().Y(), Where().Z(), Where().Facing());
    m_hasPendingCommit = true;
    return true;
}

/**
 * @brief Checks whether the unit is in a chair or sit state.
 *
 * @return True if the unit is sitting; otherwise, false.
 */
bool Unit::IsSitState() const
{
    uint8 s = getStandState();
    return
        s == UNIT_STAND_STATE_SIT_CHAIR        || s == UNIT_STAND_STATE_SIT_LOW_CHAIR  ||
        s == UNIT_STAND_STATE_SIT_MEDIUM_CHAIR || s == UNIT_STAND_STATE_SIT_HIGH_CHAIR ||
        s == UNIT_STAND_STATE_SIT;
}

/**
 * @brief Checks whether the unit is considered standing.
 *
 * @return True if the unit is standing; otherwise, false.
 */
bool Unit::IsStandState() const
{
    uint8 s = getStandState();
    return !IsSitState() && s != UNIT_STAND_STATE_SLEEP && s != UNIT_STAND_STATE_KNEEL;
}

/**
 * @brief Sets the current stand state and notifies the client when needed.
 *
 * @param state The new stand state value.
 */
void Unit::SetStandState(uint8 state)
{
    SetByteValue(UNIT_FIELD_BYTES_1, 0, state);

    if (IsStandState())
    {
        RemoveAurasWithInterruptFlags(AURA_INTERRUPT_FLAG_NOT_SEATED);
    }

    if (GetTypeId() == TYPEID_PLAYER)
    {
        SendStandStateUpdate(state);
    }
}

/**
 * @brief Tells a player's own client its stand state.
 *
 * @param state The stand state set.
 */
void Unit::SendStandStateUpdate(uint8 state)
{
    if (ClientCallbacks const* client = m_clientCallbacks)
    {
        StandStateFact fact;
        fact.state = state;
        ReportClientFact(client->standState, fact);
    }
}

/**
 * @brief Checks whether the unit is currently polymorphed.
 *
 * @return True if the transform is a polymorph effect; otherwise, false.
 */
bool Unit::IsPolymorphed() const
{
    return GetSpellSpecific(getTransForm()) == SPELL_MAGE_POLYMORPH;
}

/**
 * @brief Sets the display model and refreshes related model data.
 *
 * @param modelId The model display identifier.
 */
void Unit::SetDisplayId(uint32 modelId)
{
    SetUInt32Value(UNIT_FIELD_DISPLAYID, modelId);

    UpdateModelData();

    if (Pet* pet = ToPet(this))
    {
        if (!pet->isControlled())
        {
            return;
        }
        Unit* owner = GetOwner();
        if (owner && (owner->GetTypeId() == TYPEID_PLAYER))
        {
            ReportGroupFact(InstalledGroupCallbacks(owner->m_groupCallbacks).stat, GroupStatFact{GROUP_UPDATE_FLAG_PET_MODEL_ID});
        }
    }
}

/**
 * @brief Updates bounding radius and combat reach from the current display model.
 */
void Unit::UpdateModelData()
{
    if (CreatureModelInfo const* modelInfo = sObjectMgr.GetCreatureModelInfo(GetDisplayId()))
    {
        // we expect values in database to be relative to scale = 1.0
        SetFloatValue(UNIT_FIELD_BOUNDINGRADIUS, GetObjectScale() * modelInfo->bounding_radius);

        // never actually update combat_reach for player, it's always the same. Below player case is for initialization
        if (GetTypeId() == TYPEID_PLAYER)
        {
            SetFloatValue(UNIT_FIELD_COMBATREACH, 1.5f);
        }
        else
        {
            SetFloatValue(UNIT_FIELD_COMBATREACH, GetObjectScale() * modelInfo->combat_reach);
        }
    }
}

/**
 * @brief Selects a random unfriendly target within line of sight.
 *
 * @param except An optional unit to exclude.
 * @param radius The search radius.
 * @return A random matching target, or null if none are found.
 */
Unit* Unit::SelectRandomUnfriendlyTarget(Unit* except /*= NULL*/, float radius /*= ATTACK_DISTANCE*/) const
{
    std::list<Unit*> targets;

    MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck u_check(this, radius);
    MaNGOS::UnitListSearcher<MaNGOS::AnyUnfriendlyUnitInObjectRangeCheck> searcher(targets, u_check);
    Cell::VisitAllObjects(this, searcher, radius);

    // remove current target
    if (except)
    {
        targets.remove(except);
    }

    // remove not LoS targets
    for (std::list<Unit*>::iterator tIter = targets.begin(); tIter != targets.end();)
    {
        if (!HasLineOfSight(*this, *(*tIter)))
        {
            std::list<Unit*>::iterator tIter2 = tIter;
            ++tIter;
            targets.erase(tIter2);
        }
        else
        {
            ++tIter;
        }
    }

    // no appropriate targets
    if (targets.empty())
    {
        return NULL;
    }

    // select random
    uint32 rIdx = urand(0, targets.size() - 1);
    std::list<Unit*>::const_iterator tcIter = targets.begin();
    for (uint32 i = 0; i < rIdx; ++i)
    {
        ++tcIter;
    }

    return *tcIter;
}

/**
 * @brief Selects a random friendly target within line of sight.
 *
 * @param except An optional unit to exclude.
 * @param radius The search radius.
 * @return A random matching target, or null if none are found.
 */
Unit* Unit::SelectRandomFriendlyTarget(Unit* except /*= NULL*/, float radius /*= ATTACK_DISTANCE*/) const
{
    std::list<Unit*> targets;

    MaNGOS::AnyFriendlyUnitInObjectRangeCheck u_check(this, radius);
    MaNGOS::UnitListSearcher<MaNGOS::AnyFriendlyUnitInObjectRangeCheck> searcher(targets, u_check);

    Cell::VisitAllObjects(this, searcher, radius);

    // remove current target
    if (except)
    {
        targets.remove(except);
    }

    // remove not LoS targets
    for (std::list<Unit*>::iterator tIter = targets.begin(); tIter != targets.end();)
    {
        if (!HasLineOfSight(*this, *(*tIter)))
        {
            std::list<Unit*>::iterator tIter2 = tIter;
            ++tIter;
            targets.erase(tIter2);
        }
        else
        {
            ++tIter;
        }
    }

    // no appropriate targets
    if (targets.empty())
    {
        return NULL;
    }

    // select random
    uint32 rIdx = urand(0, targets.size() - 1);
    std::list<Unit*>::const_iterator tcIter = targets.begin();
    for (uint32 i = 0; i < rIdx; ++i)
    {
        ++tcIter;
    }

    return *tcIter;
}

/**
 * @brief Applies or removes a percentage modifier to attack speed.
 *
 * @param att The attack type to modify.
 * @param val The percentage value.
 * @param apply True to apply the modifier; false to remove it.
 */
void Unit::ApplyAttackTimePercentMod(WeaponAttackType att, float val, bool apply)
{
    if (val > 0)
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], val, !apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME + att, val, !apply);
    }
    else
    {
        ApplyPercentModFloatVar(m_modAttackSpeedPct[att], -val, apply);
        ApplyPercentModFloatValue(UNIT_FIELD_BASEATTACKTIME + att, -val, apply);
    }
}

/**
 * @brief Applies or removes a percentage modifier to cast speed.
 *
 * @param val The percentage value.
 * @param apply True to apply the modifier; false to remove it.
 */
void Unit::ApplyCastTimePercentMod(float val, bool apply)
{
    if (val > 0)
    {
        ApplyPercentModFloatValue(UNIT_MOD_CAST_SPEED, val, !apply);
    }
    else
    {
        ApplyPercentModFloatValue(UNIT_MOD_CAST_SPEED, -val, apply);
    }
}

struct SetPhaseMaskHelper
{
    explicit SetPhaseMaskHelper(uint32 _phaseMask) : phaseMask(_phaseMask) {}
    void operator()(Unit* unit) const { unit->SetPhaseMask(phaseMask, true); }
    uint32 phaseMask;
};

void Unit::SetPhaseMask(uint32 newPhaseMask, bool update)
{
    if (newPhaseMask == GetPhaseMask())
    {
        return;
    }

    // first move to both phase for proper update controlled units
    WorldObject::SetPhaseMask(GetPhaseMask() | newPhaseMask, false);

    if (IsInWorld())
    {
        RemoveNotOwnTrackedTargetAuras(newPhaseMask);       // we can lost access to caster or target

        // all controlled except not owned charmed units
        CallForAllControlledUnits(SetPhaseMaskHelper(newPhaseMask), CONTROLLED_PET | CONTROLLED_GUARDIANS | CONTROLLED_MINIPET | CONTROLLED_TOTEMS);
    }

    WorldObject::SetPhaseMask(newPhaseMask, update);
}

/**
 * @brief Teleports the unit to a nearby destination on the current map.
 *
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 * @param orientation The destination facing.
 * @param casting True when the move originates from a spell cast.
 */
void Unit::NearTeleportTo(float x, float y, float z, float orientation, bool casting /*= false*/)
{
    DisableSpline();

    if (GetTypeId() == TYPEID_PLAYER)
    {
        TeleportNear(x, y, z, orientation, casting);
    }
    else
    {
        // Creature relocation acts like instant movement: the selected behaviour expects the
        // interrupt/reset pair around it to react properly.
        GetMotionMaster()->RelocateSelected(x, y, z, orientation);
    }
}

/**
 * @brief Starts spline movement to a destination at a specific speed.
 *
 * @param x The destination X coordinate.
 * @param y The destination Y coordinate.
 * @param z The destination Z coordinate.
 * @param speed The movement speed.
 * @param generatePath True to generate a path.
 * @param forceDestination True to force the exact destination.
 */
void Unit::MonsterMoveWithSpeed(float x, float y, float z, float speed, bool generatePath, bool forceDestination)
{
    Movement::MoveSplineInit init(*this);
    init.MoveTo(x, y, z, generatePath, forceDestination);
    init.SetVelocity(speed);
    init.Launch();
}

struct SetPvPHelper
{
    explicit SetPvPHelper(bool _state) : state(_state) {}
    void operator()(Unit* unit) const { unit->SetPvP(state); }
    bool state;
};

/**
 * @brief Enables or disables PvP state for the unit and controlled entities.
 *
 * @param state True to enable PvP; false to disable it.
 */
void Unit::SetPvP(bool state)
{
    if (state)
    {
        SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_PVP);
    }
    else
    {
        RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_PVP);
    }

    CallForAllControlledUnits(SetPvPHelper(state), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

struct SetFFAPvPHelper
{
    explicit SetFFAPvPHelper(bool _state) : state(_state) {}
    void operator()(Unit* unit) const { unit->SetFFAPvP(state); }
    bool state;
};

void Unit::SetFFAPvP(bool state)
{
    if (state)
    {
        SetByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
    }
    else
    {
        RemoveByteFlag(UNIT_FIELD_BYTES_2, 1, UNIT_BYTE2_FLAG_FFA_PVP);
    }

    CallForAllControlledUnits(SetFFAPvPHelper(state), CONTROLLED_PET | CONTROLLED_TOTEMS | CONTROLLED_GUARDIANS | CONTROLLED_CHARM);
}

void Unit::RestoreOriginalFaction()
{
    setFactionForRace(getRace());
}

void Unit::KnockBackFrom(Unit* target, float horizontalSpeed, float verticalSpeed)
{
    float angle = this == target ? Where().Facing() + M_PI_F : target->Where().BearingTo(this->Where());
    KnockBackWithAngle(angle, horizontalSpeed, verticalSpeed);
}

void Unit::KnockBackWithAngle(float angle, float horizontalSpeed, float verticalSpeed)
{
    if (GetTypeId() == TYPEID_PLAYER)
    {
        Motion::KnockBackParams params;
        // The direction is computed in double and narrowed to float.
        params.directionX = float(cos(double(angle)));
        params.directionY = float(sin(double(angle)));
        params.horizontal = horizontalSpeed;
        params.vertical = -verticalSpeed;   // as the wire carries it
        SendEmissions(MotionState().Apply(Motion::KnockBackChange(params), GameTime::GetGameTimeMS()));
    }
    else
    {
        float vsin = sin(angle);
        float vcos = cos(angle);
        float moveTimeHalf = verticalSpeed / Movement::gravity;
        float max_height = -Movement::computeFallElevation(moveTimeHalf, false, -verticalSpeed);

        float dis = 2 * moveTimeHalf * horizontalSpeed;
        float ox, oy, oz;
        ox = Where().X();
        oy = Where().Y();
        oz = Where().Z();
        float fx = ox + dis * vcos;
        float fy = oy + dis * vsin;
        float fz = oz + 0.5f;
        GetMap()->GetHitPosition(ox, oy, oz + 0.5f, fx, fy, fz, GetPhaseMask(), -0.5f);
        ClampToAllowedZ(*this, fx, fy, fz);
        GetMotionMaster()->MoveJump(fx, fy, fz, horizontalSpeed, max_height);
    }
}

bool Unit::IsIgnoreUnitState(SpellEntry const* spell, IgnoreUnitState ignoreState)
{
    Unit::AuraList const& stateAuras = GetAurasByType(SPELL_AURA_IGNORE_UNIT_STATE);
    for (Unit::AuraList::const_iterator itr = stateAuras.begin(); itr != stateAuras.end(); ++itr)
    {
        if ((*itr)->GetModifier()->m_miscvalue == ignoreState)
        {
            // frozen state absent ignored for all spells
            if (ignoreState == IGNORE_UNIT_TARGET_NON_FROZEN)
            {
                return true;
            }

            if ((*itr)->isAffectedOnSpell(spell))
            {
                return true;
            }
        }
    }

    return false;
}

/**
 * @brief Verifies chained cast limits and increments the cast counter.
 *
 * @return True if casting may continue; otherwise, false.
 */
bool Unit::CheckAndIncreaseCastCounter()
{
    uint32 maxCasts = sGameConfig.getConfig(CONFIG_UINT32_MAX_SPELL_CASTS_IN_CHAIN);

    if (maxCasts && m_castCounter >= maxCasts)
    {
        return false;
    }

    ++m_castCounter;
    return true;
}

class RelocationNotifyEvent : public BasicEvent
{
    public:
        RelocationNotifyEvent(Unit& owner) : BasicEvent(), m_owner(owner)
        {
            m_owner._SetAINotifyScheduled(true);
        }

        bool Execute(uint64 /*e_time*/, uint32 /*p_time*/)
        {
            float radius = MAX_CREATURE_ATTACK_RADIUS * sGameConfig.getConfig(CONFIG_FLOAT_RATE_CREATURE_AGGRO);
            if (m_owner.GetTypeId() == TYPEID_PLAYER)
            {
                MaNGOS::PlayerRelocationNotifier notify((Player&)m_owner);
                Cell::VisitAllObjects(&m_owner, notify, radius);
            }
            else // if (m_owner.GetTypeId() == TYPEID_UNIT)
            {
                MaNGOS::CreatureRelocationNotifier notify((Creature&)m_owner);
                Cell::VisitAllObjects(&m_owner, notify, radius);
            }
            m_owner._SetAINotifyScheduled(false);
            return true;
        }

        void Abort(uint64)
        {
            m_owner._SetAINotifyScheduled(false);
        }

    private:
        Unit& m_owner;
};

/**
 * @brief Schedules deferred AI relocation notifications.
 *
 * @param delay The delay before the notification event executes.
 */
void Unit::ScheduleAINotify(uint32 delay)
{
    if (!IsAINotifyScheduled())
    {
        m_Events.AddEvent(new RelocationNotifyEvent(*this), m_Events.CalculateTime(delay));
    }
}

/**
 * @brief Handles relocation updates after the unit position changes.
 */
void Unit::OnRelocated()
{
    // switch to use Geometry::Vector3 is good idea, maybe
    float dx = m_last_notified_position.x - Where().X();
    float dy = m_last_notified_position.y - Where().Y();
    float dz = m_last_notified_position.z - Where().Z();
    float distsq = dx * dx + dy * dy + dz * dz;
    if (distsq > GameConfig::GetRelocationLowerLimitSq())
    {
        m_last_notified_position.x = Where().X();
        m_last_notified_position.y = Where().Y();
        m_last_notified_position.z = Where().Z();

        GetViewPoint().Call_UpdateVisibilityForOwner();
        UpdateObjectVisibility();
    }
    ScheduleAINotify(GameConfig::GetRelocationAINotifyDelay());
}

/*
 * @param entry             entry of the vehicle kit
 * @param overwriteNpcEntry use to select behaviour (like accessory) for this entry instead of GetEntry()'s result
 */
void Unit::SetVehicleId(uint32 entry, uint32 overwriteNpcEntry)
{
    delete m_vehicleInfo;

    if (entry)
    {
        VehicleEntry const* ventry = sVehicleStore.LookupEntry(entry);
        MANGOS_ASSERT(ventry != NULL);

        m_vehicleInfo = new VehicleInfo(this, ventry, overwriteNpcEntry);
        m_updateFlag |= UPDATEFLAG_VEHICLE;
    }
    else
    {
        m_vehicleInfo = NULL;
        m_updateFlag &= ~UPDATEFLAG_VEHICLE;
    }

    if (GetTypeId() == TYPEID_PLAYER)
    {
        WorldPacket data(SMSG_SET_VEHICLE_REC_ID, 16);
        data << GetPackGUID();
        data << uint32(entry);
        SendMessageToSet(&data, true);
    }
}

/**
 * @brief Advances active spline movement and updates world position.
 *
 * @param t_diff The elapsed update time in milliseconds.
 */
void Unit::UpdateSplineMovement(uint32 t_diff)
{
    enum
    {
        POSITION_UPDATE_DELAY = 400,
    };

    if (movespline->Finalized())
    {
        return;
    }

    movespline->updateState(t_diff);
    bool arrived = movespline->Finalized();

    if (arrived)
    {
        DisableSpline();
    }

    m_movesplineTimer.Update(t_diff);
    if (m_movesplineTimer.Passed() || arrived)
    {
        m_movesplineTimer.Reset(POSITION_UPDATE_DELAY);
        Movement::Location loc = movespline->ComputePosition();

        if (IsBoarded())
        {
            Geometry::Placement deckPose;
            deckPose.EnterFrame(GetTransportInfo()->Seat().CurrentFrame(),
                                Geometry::Vector3(loc.x, loc.y, loc.z), loc.orientation);
            GetTransportInfo()->SetSeatPose(deckPose);
        }
        else
        {
            SetPosition(loc.x, loc.y, loc.z, loc.orientation);
        }
    }
}

/**
 * @brief Disables spline movement flags and interrupts the current spline.
 */
void Unit::DisableSpline()
{
    // A spline that ran out is finished, not cut; only a live one is interrupted here.
    if (!movespline->Finalized())
    {
        movespline->_Interrupt();
    }
}

bool Unit::IsSplineEnabled() const
{
    return movespline->Initialized() && !movespline->Finalized();
}

bool Unit::IsInWorgenForm(bool inPermanent) const
{
    AuraList const& vTransformAuras = GetAurasByType(SPELL_AURA_WORGEN_TRANSFORM);
    for (AuraList::const_iterator itr = vTransformAuras.begin(); itr != vTransformAuras.end(); ++itr)
        if (!inPermanent || (*itr)->GetHolder()->IsPermanent())
        {
            return true;
        }

    return false;
}

bool Unit::HasWorgenForm() const
{
    return HasAuraType(SPELL_AURA_ALLOW_WORGEN_TRANSFORM);
}

void Unit::SendCollisionHeightUpdate(float height)
{
    if (GetTypeId() != TYPEID_PLAYER)
    {
        return;
    }
    // The 4.3.4 layout with a real counter (the legacy writer sent the WotLK shape on a
    // game-time counter and threw this parameter away for a second lookup). Both callers
    // are the mount and dismount paths: reason 1, "mount".
    SendEmissions(m_motion->Apply(Motion::HeightChange(height, 1), GameTime::GetGameTimeMS()));
}

// This will create a new creature and set the current unit as the controller of that new creature
Creature* Unit::TakePossessOf(SpellEntry const* spellEntry, SummonPropertiesEntry const* /*summonProp*/, SpellEffectEntry const* spellEffect, float x, float y, float z, float ang)
{
    int32 const& creatureEntry = spellEffect->EffectMiscValue_0;
    CreatureInfo const* cinfo = ObjectMgr::GetCreatureTemplate(creatureEntry);
    if (!cinfo)
    {
        sLog.outErrorDb("WorldObject::SummonCreature: Creature (Entry: %u) not existed for summoner: %s. ", creatureEntry, GetGuidStr().c_str());
        return NULL;
    }

    if (GetCharm())
    {
        sLog.outError("Unit::TakePossessOf> There is already a charmed creature for %s its : %s. ", GetGuidStr().c_str(), GetCharm()->GetGuidStr().c_str());
        return NULL;
    }

    TemporarySummon* pCreature = new TemporarySummon(GetObjectGuid());

    CreatureCreatePos pos(GetMap(), x, y, z, ang, GetPhaseMask());

    if (x == 0.0f && y == 0.0f && z == 0.0f)
    {
        pos = CreatureCreatePos(this, Where().Facing(), CONTACT_DISTANCE, ang);
    }

    if (!pCreature->Create(GetMap()->GenerateLocalLowGuid(cinfo->GetHighGuid()), pos, cinfo))
    {
        delete pCreature;
        return NULL;
    }

    bool player = GetTypeId() == TYPEID_PLAYER;

    pCreature->setFaction(getFaction());                                // set same faction than player
    pCreature->SetSpawn(pos);                                    // set spawn coord
    pCreature->SetCharmerGuid(GetObjectGuid());                         // save guid of the charmer
    pCreature->SetUInt32Value(UNIT_CREATED_BY_SPELL, spellEntry->ID);   // set the spell id used to create this (may be used for removing corresponding aura
    pCreature->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);  // set flag for client that mean this unit is controlled by a player
    pCreature->GetMotionMaster()->Inhibit(Motion::Inhibition::Possessed, Motion::InhibitSource(Motion::SourceDomain::Possession, GetObjectGuid().GetCounter())); // also set internal block state
    pCreature->SelectLevel(getLevel());                                 // set level to same level than summoner TODO:: not sure its always the case...
    pCreature->SetLinkedToOwnerAura(TEMPSPAWN_LINKED_AURA_OWNER_CHECK | TEMPSPAWN_LINKED_AURA_REMOVE_OWNER); // set what to do if linked aura is removed or the creature is dead.
    pCreature->SetWalk(IsWalking(), true);                              // sync the walking state with the summoner

    // important before adding to the map!
    SetCharmGuid(pCreature->GetObjectGuid());                           // save guid of charmed creature

    pCreature->SetSummonProperties(TEMPSPAWN_CORPSE_TIMED_DESPAWN, 5000); // set 5s corpse decay
    GetMap()->Add<Creature>(pCreature);                                 // create the creature in the client

    // Give the control to the player
    if (player)
    {
        SetCameraView(pCreature);                         // modify camera view to the creature view
        SetClientControl(pCreature, 1);                         // transfer client control to the creature
        SendForcedObjectUpdate();                               // we have to update client data here to avoid problem with the "release spirit" windows reappear.
    }

    // initialize AI
    pCreature->AIM_Initialize();

    if (player)
    {
        // Initialize pet bar
        if (CharmInfo* charmInfo = pCreature->InitCharmInfo(pCreature))
        {
            charmInfo->InitPossessCreateSpells();
        }
        PossessSpellInitialize();
    }
    else
    {
        // fire just summoned hook
        if (GetTypeId() == TYPEID_UNIT && AI())
        {
            AI()->JustSummoned(pCreature);
        }
    }

    // Creature Linking, Initial load is handled like respawn
    if (pCreature->IsLinkingEventTrigger())
    {
        GetMap()->GetCreatureLinkingHolder()->DoCreatureLinkingEvent(LINKING_EVENT_RESPAWN, pCreature);
    }

    // return the creature therewith the summoner has access to it
    return pCreature;
}

bool Unit::TakePossessOf(Unit* possessed)
{
    bool player = false;
    if (GetTypeId() == TYPEID_PLAYER)
    {
        player = true;
    }

    possessed->SetFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    possessed->SetCharmerGuid(GetObjectGuid());
    // The block's client-root projection runs inside this Inhibit, and it reads the AUTHORITY
    // (Unit::IsClientMover), which SetClientControl below has not handed over yet: a body
    // stunned before the take is projected here as the server-driven unit it still is, and it is
    // WorldSession::GrantMover's own RefreshClientRoot that roots it a few lines down. Before
    // 2026-09-21 the projection read the charmer set on the line above instead, which is why the
    // charmer used to have to come first.
    possessed->GetMotionMaster()->Inhibit(Motion::Inhibition::Possessed, Motion::InhibitSource(Motion::SourceDomain::Possession, GetObjectGuid().GetCounter()));
    possessed->setFaction(getFaction());

    SetCharm(possessed);

    Creature* possessedCreature = possessed->ToCreature();

    // A BODY THAT WAS SITTING CANNOT BE STEERED (live test 2026-09-22, B1 bonus): mind-control
    // an AFK creature and the possessor gets the camera and the mover but no right-click turn,
    // because the client refuses to turn a unit whose stand state is not STAND and nothing on
    // this path ever stood it up. The take is the moment to do it -- before the control grant
    // below, so the body is already standing when the client is handed it, the same way
    // SpellAuraControl.cpp:509 stands a unit up as its stun takes hold.
    possessed->SetStandState(UNIT_STAND_STATE_STAND);

    if (player)
    {
        const bool ownPet = possessedCreature && possessedCreature->IsPet() && possessedCreature->GetObjectGuid() == GetPetGuid();
        if (ownPet)
        {
            // The take ends the server's control episodes (spec §7): the pet answers its
            // master now, and the grant below needs the flee and confuse states gone.
            possessed->GetMotionMaster()->CancelControl(Motion::Kind::Fear);
            possessed->GetMotionMaster()->CancelControl(Motion::Kind::Confused);
        }

        SetCameraView(possessed);
        SetClientControl(possessed, 1);
        SendForcedObjectUpdate();

        if (ownPet)
        {
            possessed->StopMoving();
            possessed->GetMotionMaster()->Clear(false);
            possessed->GetMotionMaster()->MoveIdle();
            return true;
        }
        else if (CharmInfo* charmInfo = possessed->InitCharmInfo(possessed))
        {
            charmInfo->InitPossessCreateSpells();
            charmInfo->SetReactState(REACT_PASSIVE);
            charmInfo->SetCommandState(COMMAND_STAY);
        }
        PossessSpellInitialize();
    }

    possessed->CombatStop(true);
    possessed->DeleteThreatList();
    possessed->GetHostileRefManager().deleteReferences();

    if (possessedCreature)
    {
        possessedCreature->AIM_Initialize();
    }
    else if (Player* possessedPlayer = possessed->ToPlayer())
    {
        possessedPlayer->SetClientControl(possessed, 0);
    }
    return true;
}

void Unit::ResetControlState(bool attackCharmer /*= true*/)
{
    bool player = false;
    if (GetTypeId() == TYPEID_PLAYER)
    {
        player = true;
    }

    Unit* possessed = GetCharm();

    if (!possessed)
    {
        if (player)
        {
            ResetCameraView();
            // A flying body keeps its control revoked until the landing or the abort grants
            // (P5-B family 5): a possession ending mid-flight hands back the camera alone.
            if (!IsTaxiFlying())
            {
                SetClientControl(this, 1);
            }
        }
        return;
    }

    // THE POSSESSED CAN BE A PLAYER: spell 605, Mind Control -- SPELL_AURA_MOD_POSSESS on
    // TARGET_CHAIN_DAMAGE (SpellEffect.dbc, 4.3.4a) -- so its expiry, Aura::HandleModPossess
    // (apply=false) -> `caster->ResetControlState()`, reaches here with GetCharm() answering
    // a Player. The pet question below is asked through Unit::IsPet(), which answers false for
    // anything that is not a Creature (decoupling D5f), so the read is typed; the Creature
    // pointer is a checked one, which makes the `else if (possessedCreature)` tail a real
    // guard instead of a tautology.
    Creature* possessedCreature = possessed->ToCreature();

    possessed->RemoveFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED);
    possessed->SetCharmerGuid(ObjectGuid());
    // The projection runs inside this Uninhibit too, and the authority it reads is still the
    // possessor's session: SetClientControl below has not revoked yet, so a body still stunned
    // stays client-rooted through here and it is WorldSession::RevokeMover's own
    // RefreshClientRoot that takes the root off. Before 2026-09-21 the projection read the
    // charmer cleared on the line above, and unrooted the body here instead.
    possessed->GetMotionMaster()->Uninhibit(Motion::Inhibition::Possessed, Motion::InhibitSource(Motion::SourceDomain::Possession, GetObjectGuid().GetCounter()));
    SetCharmGuid(ObjectGuid());

    if (player)
    {
        // The unit's revoke, the camera, then the player's own grant: today's release
        // sent only the first and left the client to recover on its own.
        SetClientControl(possessed, 0);
        ResetCameraView();
        if (!IsTaxiFlying())   // a flying body: the landing or the abort grants (P5-B family 5)
        {
            SetClientControl(this, 1);
        }

        if (possessed->IsPet() && possessed->GetObjectGuid() == GetPetGuid())
        {
            // out of range pet dismissed
            if (!InReach(*possessed, *this, possessed->GetMap()->GetVisibilityDistance()))
            {
                RemovePet(PET_SAVE_REAGENTS);
            }
            else
            {
                possessed->GetMotionMaster()->MoveFollow(this, PET_FOLLOW_DIST, PET_FOLLOW_ANGLE);
            }

            return;
        }
        else
        {
            RemovePetActionBar();
        }
    }

    possessed->CombatStop(true);
    possessed->DeleteThreatList();
    possessed->GetHostileRefManager().deleteReferences();

    if (Player* possessedPlayer = possessed->ToPlayer())
    {
        possessedPlayer->setFactionForRace(possessedPlayer->getRace());
        if (!possessedPlayer->IsTaxiFlying())   // a possessed passenger: the flight went on under the possession, and its landing or abort grants
        {
            possessedPlayer->SetClientControl(possessedPlayer, 1);
        }
    }
    else if (possessedCreature)
    {
        if (attackCharmer)
        {
            CreatureInfo const* cinfo = possessedCreature->GetCreatureInfo();
            possessedCreature->setFaction(cinfo->FactionAlliance);
            possessedCreature->AIM_Initialize();
            possessedCreature->AttackedBy(this);
        }
    }
}


