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

#include "combat/HitInheritance.h"
#include "spells/classes/mage/Mage.h"
#include "spells/data/SpellMgr.h"
#include "spells/SpellScriptQueries.h"
#include "ai/CreatureAI.h"
#include "entities/Creature.h"
#include "Map.h"
#include "GridMap.h"
#include "entities/motion/MotionMaster.h"
#include "DBCStores.h"
#include "Utilities/Util.h"
#include "movement/MoveSpline.h"
#include "movement/typedefs.h"

#include <algorithm>
#include <cmath>

namespace
{
    /// The orbs of the mage's spells, one row each: Flame Orb.
    Mage::OrbSpells const ORBS[] =
    {
        { 82731, 84765, 44214, 82690, 82734, 82739, 82736, 83619, { 18459, 18460, 54734 } },
    };

    /// The name the orb's creature AI is registered under.
    char const* const ORB_AI = "OrbAI";

    /// The id of the point the orb flies to.
    uint32 const ORB_PATH_POINT = 1;

    /// How far from its point the orb may stand for its flight to count as ended there, in yards.
    float const ORB_PATH_END = 1.0f;

    /// The id of the glide the orb makes down to its hover line.
    uint32 const ORB_GLIDE = 2;

    /// The id of each short leg the orb flies up to its hover line over rising ground.
    uint32 const ORB_CLIMB = 3;

    /// How long a climb leg may have left before the next one is laid from where the orb is, in seconds.
    float const ORB_CLIMB_RELAY_SEC = 0.15f;

    /// How long a glide may have run before the orb takes a finished movement without its inform as the glide's end, in
    /// milliseconds.
    uint32 const ORB_GLIDE_LOST_MS = 1000;

    /// The orb's owner when he may have the orb act for him (Mage::OrbOwnerActs): looked up on the orb's own map by
    /// the guid of his summon; NULL otherwise.
    Unit* ActingOwner(Creature const& orb)
    {
        Map* map = orb.FindMap();
        Unit* owner = map ? map->GetUnit(orb.GetSummonerGuid()) : NULL;
        bool const found = owner != NULL;
        if (!Mage::OrbOwnerActs(found, found && owner->IsAlive(), found && owner->IsInWorld(),
                                found && owner->FindMap() == map))
        {
            return NULL;
        }
        return owner;
    }

    /// 82731: Flame Orb: the mage casts the summon of his orb (84765), triggered. The script ends the effect.
    SpellHandlerOutcome<void> FlameOrbEffect(DummyEffectContext& ctx)
    {
        Mage::OrbSpells const* orb = Mage::OrbSpellsFor(ctx.spell.ID);
        if (ctx.effIndex == EFFECT_INDEX_0 && orb)
        {
            ctx.CastTriggered(ctx.caster, orb->summonSpell);
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 82734: the orb's scan: of the enemies within its radius keeps the closest one the orb may strike and sees
    /// (Mage::KeepClosestOrbTarget), or none. Sight is taken from the enemy to the orb, as the hit's target check takes
    /// it (Spell::CheckTarget).
    SpellHandlerOutcome<void> OrbScanTargets(SelectTargetsContext& ctx)
    {
        Mage::KeepClosestOrbTarget(ctx.caster, ctx.targets,
            [&ctx](Unit const& unit) { return HasLineOfSight(unit, ctx.caster); });
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 82734: the orb's scan, for the enemy it kept. On its first enemy the orb snares itself (82736) and counts
    /// that scan as scan 0. While it holds the snare, each scan with an enemy counts one more, and on the scans
    /// Mage::OrbHitDue names (scan 0 and every second scan of the 500 ms aura after it, for the 30 scans the snare
    /// lasts) the orb casts its hit (82739) at the enemy, with its summoner as the original caster, when its
    /// summoner is alive and in the world on its map (Mage::OrbOwnerActs). The aura's period carries its remainder
    /// from tick to tick, so the hits keep one second apart on any update rate. The count is the script word of
    /// effect 0 of the orb's own aura (82690), and the word of its effect 1 is 1 once an enemy was found. An orb
    /// without that aura does nothing. The script ends the effect.
    SpellHandlerOutcome<void> OrbScanEffect(DummyEffectContext& ctx)
    {
        Creature* orb = ctx.caster.ToCreature();
        Mage::OrbSpells const* spells = orb ? Mage::OrbSpellsFor(orb->GetUInt32Value(UNIT_CREATED_BY_SPELL)) : NULL;
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.target || !spells)
        {
            return SpellHandlerOutcome<void>::Return();
        }
        int32* scans = ScriptWordOf(*orb, spells->orbAura, EFFECT_INDEX_0);
        int32* found = ScriptWordOf(*orb, spells->orbAura, EFFECT_INDEX_1);
        if (!scans || !found)
        {
            return SpellHandlerOutcome<void>::Return();
        }
        if (!*found)
        {
            *found = 1;
            *scans = 0;
            orb->CastSpell(orb, spells->snareSpell, true);
        }
        if (!orb->HasAura(spells->snareSpell))
        {
            return SpellHandlerOutcome<void>::Return();
        }
        uint32 const scan = uint32((*scans)++);
        if (!Mage::OrbHitDue(scan, Mage::OrbCadenceFor(*spells)))
        {
            return SpellHandlerOutcome<void>::Return();
        }
        Unit* summoner = ActingOwner(*orb);
        if (!summoner)
        {
            return SpellHandlerOutcome<void>::Return();
        }
        orb->CastSpell(ctx.target, spells->hitSpell, true, NULL, NULL, summoner->GetObjectGuid());
        return SpellHandlerOutcome<void>::Return();
    }

    /// The orb of a mage, owned by him and of his level: glides forward on the slow-fall curve to its hover height above
    /// the floor when it is summoned higher or the floor falls away under it, then flies straight ahead from where it
    /// stands at its run speed, in short legs up to its hover line where the floor rises ahead of it, slowed by its
    /// snare when it finds an enemy; when the snare runs out (its firing is over), or for an orb that found nothing at
    /// its summon's end (the summon's own end, or its life's when it is in combat), when its summoner is alive and on its
    /// map, rolls the proc chance of the summoner's explosion talent for the explosion (83619), cast with the summoner
    /// as the original caster, and goes. It attacks nothing, follows nothing and evades nothing.
    class OrbAI : public CreatureAI
    {
        public:
            explicit OrbAI(Creature* orb) : CreatureAI(orb), m_spells(NULL), m_lifeMs(0), m_ended(false), m_snared(false), m_flying(false), m_gliding(false), m_climbing(false), m_glideMs(0), m_facing(0.0f), m_legX(0.0f), m_legY(0.0f), m_legZ(0.0f) {}

            /// The orb's start: its row, by the spell that created it; its summoner as its owner, so the orb counts as
            /// his and its scan takes the units he may attack, as his own area spells take them; its summoner's level,
            /// as a totem takes its owner's, so its hit and its explosion roll against their target by his level; its
            /// life, the summon spell's duration; and its glide to its hover height above the floor when it was summoned
            /// higher, or else its flight (StartFlight). An orb of no row ends here and does nothing.
            void Reset() override
            {
                m_spells = Mage::OrbSpellsFor(m_creature->GetUInt32Value(UNIT_CREATED_BY_SPELL));
                m_ended = m_spells == NULL;
                if (m_ended)
                {
                    return;
                }
                m_creature->SetOwnerGuid(m_creature->GetSummonerGuid());
                if (Unit* summoner = ActingOwner(*m_creature))
                {
                    m_creature->SetLevel(summoner->getLevel());
                }
                Combat::TakeOwnerHitSnapshot(*m_creature);
                m_lifeMs = GetSpellDuration(sSpellStore.LookupEntry(m_spells->summonSpell));
                if (!StartGlide(false))
                {
                    StartFlight();
                }
            }

            /// Starts the orb on its first update, when the summon has set the spell that created it; then counts its
            /// life down, notes its snare, and ends it when the snare has run out or the life has (Mage::OrbEndDue).
            void UpdateAI(uint32 const diff) override
            {
                if (m_ended)
                {
                    return;
                }
                if (!m_spells)
                {
                    Reset();
                    if (m_ended)
                    {
                        return;
                    }
                }
                m_lifeMs -= int32(diff);
                bool const snared = m_creature->HasAura(m_spells->snareSpell);
                if (Mage::OrbEndDue(m_snared, snared, m_lifeMs))
                {
                    End(true);
                    return;
                }
                m_snared = m_snared || snared;
                if (m_gliding)
                {
                    m_glideMs += diff;
                    if (m_glideMs > ORB_GLIDE_LOST_MS && m_creature->movespline->Finalized())
                    {
                        StartFlight();
                    }
                }
                else if (m_flying && !StartGlide(true))
                {
                    FollowGround();
                }
            }

            /// The summon's own end (TemporarySummon::UnSummon tells the orb's AI first), which an orb that found
            /// nothing reaches before its life runs out: the orb ends there, and the summon takes it away.
            void SummonedCreatureDespawn(Creature* summoned) override
            {
                if (summoned == m_creature && !m_ended && m_spells)
                {
                    End(false);
                }
            }

            /// Starts the flight when the glide ended; flies on to the point when the flight ended short of it, and from
            /// the height a climb leg reached when the leg ended.
            void MovementInform(Motion::Kind kind, uint32 id) override
            {
                if (kind == Motion::Kind::Point && id == ORB_CLIMB && !m_ended && m_spells && m_climbing)
                {
                    m_climbing = false;
                    m_goal.z = m_creature->Where().Z();
                    FlyToGoal();
                    return;
                }
                if (kind == Motion::Kind::Effect && id == ORB_GLIDE && !m_ended && m_spells && m_gliding)
                {
                    StartFlight();
                    return;
                }
                if (kind == Motion::Kind::Point && id == ORB_PATH_POINT && !m_ended && m_spells &&
                    !m_creature->Where().WithinDist(m_goal, ORB_PATH_END))
                {
                    FlyToGoal();
                }
            }

            void AttackStart(Unit* /*who*/) override {}
            void MoveInLineOfSight(Unit* /*who*/) override {}
            void EnterCombat(Unit* /*enemy*/) override {}
            void EnterEvadeMode() override {}

            /// PERMIT_BASE_SPECIAL for a creature of the orb table, PERMIT_BASE_NO for any other.
            static int Permissible(Creature const* creature)
            {
                return Mage::IsOrbCreature(creature->GetEntry()) ? PERMIT_BASE_SPECIAL : PERMIT_BASE_NO;
            }

        private:
            /// The flight: to the point the life it has left at its run speed takes it straight ahead of where it stands, at
            /// the height it stands at; the point is taken from the placement the flight starts at.
            void StartFlight()
            {
                m_gliding = false;
                m_climbing = false;
                m_flying = true;
                m_facing = m_creature->Where().Facing();
                float const distance = float(m_lifeMs > 0 ? m_lifeMs : 0) / 1000.0f * m_creature->GetSpeed(MOVE_RUN);
                m_goal = m_creature->Where().PointAhead(distance);
                FlyToGoal();
            }

            /// The glide, for an orb that hovers and falls slowly, when it is at least a hover height above its hover line
            /// (`inFlight`: two hover heights above the floor under it, from where its spline has it; at its start, from
            /// its placement). The point it goes to is Mage::OrbGlideLanding's, along the line it faces, over the floor or,
            /// for an orb that walks on water, the water's surface. The point it held is expired first, since the glide is
            /// a forced movement that would otherwise leave the flight to resume to its old goal. Never again for the
            /// snare. @return Whether a glide began.
            bool StartGlide(bool inFlight)
            {
                Map* map = m_creature->FindMap();
                float const hover = m_creature->GetFloatValue(UNIT_FIELD_HOVERHEIGHT);
                uint32 const gliderFlags = MOVEFLAG_HOVER | MOVEFLAG_SAFE_FALL;
                if (!map || hover <= 0.0f || (m_creature->WireFlags() & gliderFlags) != gliderFlags)
                {
                    return false;
                }
                Mage::OrbGlidePoint start = { m_creature->Where().X(), m_creature->Where().Y(), m_creature->Where().Z() };
                if (inFlight && m_creature->movespline && !m_creature->movespline->Finalized())
                {
                    Movement::Location const live = m_creature->movespline->ComputePosition();
                    start.x = live.x;
                    start.y = live.y;
                    start.z = live.z;
                }
                std::optional<Mage::OrbGlidePoint> const landing = Mage::OrbGlideLanding(
                    start, m_creature->Where().Facing(), m_creature->GetSpeed(MOVE_RUN), hover, FloorAt(*map));
                if (!landing)
                {
                    return false;
                }
                if (inFlight)
                {
                    m_creature->GetMotionMaster()->MovementExpired(false);
                }
                if (!m_creature->GetMotionMaster()->MoveFallTo(ORB_GLIDE, landing->x, landing->y, landing->z))
                {
                    return false;
                }
                m_gliding = true;
                m_climbing = false;
                m_glideMs = 0;
                return true;
            }

            /// The floor an orb finds under (x, y) seen from z on `map`: the ground, or for an orb that walks on water the
            /// higher of it and the water's surface (Mage::OrbGlideFloor).
            std::function<std::optional<float>(float, float, float)> FloorAt(Map& map) const
            {
                Map* const mapPtr = &map;
                uint32 const phase = m_creature->GetPhaseMask();
                bool const walksOnWater = (m_creature->WireFlags() & MOVEFLAG_WATERWALKING) != 0;
                return [mapPtr, phase, walksOnWater](float x, float y, float z)
                {
                    return Mage::OrbGlideFloor(mapPtr->Floor(phase, x, y, z),
                                               walksOnWater ? mapPtr->GetTerrain()->GetWaterLevel(x, y, z) : std::nullopt, walksOnWater);
                };
            }

            /// The climb, while the orb flies level: where the floor half a second ahead of it, along its line, lies over
            /// its hover line, lays a short leg to a hover height above that floor (Mage::OrbClimbTo), from the orb's live
            /// point. A leg that runs is followed by the next one from where the orb is once it is ORB_CLIMB_RELAY_SEC of
            /// flight or less from its end and the floor ahead still asks for one. The next leg is laid from the live
            /// point with no stop between the two, so the orb does not stand between legs. The far goal's x and y stay;
            /// its z is the height each leg ends at.
            void FollowGround()
            {
                Map* map = m_creature->FindMap();
                float const hover = m_creature->GetFloatValue(UNIT_FIELD_HOVERHEIGHT);
                if (!map || hover <= 0.0f)
                {
                    return;
                }
                bool const spline = m_creature->movespline && !m_creature->movespline->Finalized();
                float x = m_creature->Where().X();
                float y = m_creature->Where().Y();
                float z = m_creature->Where().Z();
                if (spline)
                {
                    Movement::Location const live = m_creature->movespline->ComputePosition();
                    x = live.x;
                    y = live.y;
                    z = live.z;
                }
                float const speed = m_creature->GetSpeed(MOVE_RUN);
                if (m_climbing && spline &&
                    std::sqrt((x - m_legX) * (x - m_legX) + (y - m_legY) * (y - m_legY) + (z - m_legZ) * (z - m_legZ)) >
                        ORB_CLIMB_RELAY_SEC * speed)
                {
                    return;
                }
                float const lookAhead = Mage::OrbLookAhead(speed);
                float const aheadX = x + std::cos(m_facing) * lookAhead;
                float const aheadY = y + std::sin(m_facing) * lookAhead;
                std::optional<float> const climbZ =
                    Mage::OrbClimbTo(z, FloorAt(*map)(aheadX, aheadY, z + lookAhead + hover), hover, lookAhead);
                if (!climbZ)
                {
                    return;
                }
                m_creature->GetMotionMaster()->MovementExpired(false);
                m_creature->GetMotionMaster()->MovePointOnward(ORB_CLIMB, aheadX, aheadY, *climbZ, false);
                m_climbing = true;
                m_legX = aheadX;
                m_legY = aheadY;
                m_legZ = *climbZ;
            }

            /// A straight flight to the point, with no path generated, laid at once from where the orb is.
            void FlyToGoal()
            {
                m_creature->GetMotionMaster()->MovePointOnward(ORB_PATH_POINT, m_goal.x, m_goal.y, m_goal.z, false);
            }

            /// The orb's end: the explosion, when its summoner is alive on its map, knows one of the row's explosion
            /// talents and the talent's chance rolls; then, with `despawn`, the orb despawns.
            void End(bool despawn)
            {
                m_ended = true;
                if (Unit* owner = ActingOwner(*m_creature))
                {
                    uint32 talent = 0;
                    for (uint32 spellId : m_spells->explosionTalents)
                    {
                        if (spellId && owner->HasAura(spellId))
                        {
                            talent = spellId;
                            break;
                        }
                    }
                    if (talent && roll_chance_i(int(Mage::OrbExplosionChance(talent))))
                    {
                        m_creature->CastSpell(m_creature, m_spells->explosionSpell, true, NULL, NULL, owner->GetObjectGuid());
                    }
                }
                if (despawn)
                {
                    m_creature->ForcedDespawn();
                }
            }

            Mage::OrbSpells const* m_spells;    ///< the orb's row; NULL until its start
            int32 m_lifeMs;                     ///< the milliseconds the orb has left
            bool m_ended;                       ///< the orb ended, or had no row
            bool m_snared;                      ///< the orb has held its snare: it found an enemy
            bool m_flying;                      ///< the level flight has started
            bool m_gliding;                     ///< the glide to its hover line is under way
            bool m_climbing;                    ///< a climb leg is the movement it asked for last
            uint32 m_glideMs;                   ///< the milliseconds the glide has run
            float m_facing;                     ///< the direction of the line the orb flies along
            float m_legX;                       ///< the end of the climb leg it asked for last
            float m_legY;
            float m_legZ;
            Geometry::Vector3 m_goal;           ///< the point the orb flies to
    };
}

std::optional<float> Mage::OrbGlideFloor(std::optional<float> floor, std::optional<float> water, bool walksOnWater)
{
    if (!walksOnWater || !water)
    {
        return floor;
    }
    return floor && *floor > *water ? floor : water;
}

float Mage::OrbGlideDistance(float speed, float drop)
{
    return drop > 0.0f ? speed * Movement::computeFallTime(drop, true) : 0.0f;
}

float Mage::OrbLookAhead(float speed)
{
    return std::max(1.0f, 0.5f * speed);
}

std::optional<float> Mage::OrbClimbTo(float z, std::optional<float> floorAhead, float hover, float lookAhead)
{
    if (!floorAhead)
    {
        return std::nullopt;
    }
    float const line = *floorAhead + hover;
    float const rise = line - z;
    return rise > 0.25f && rise <= lookAhead ? std::optional<float>(line) : std::nullopt;
}

std::optional<Mage::OrbGlidePoint> Mage::OrbGlideLanding(OrbGlidePoint start, float facing, float speed, float hover,
                                                         std::function<std::optional<float>(float, float, float)> const& floorAt)
{
    std::optional<float> const landingStart = MotionMaster::FallLanding(start.z, floorAt(start.x, start.y, start.z), hover, hover);
    if (!landingStart)
    {
        return std::nullopt;
    }
    float landingZ = *landingStart;
    float distance = OrbGlideDistance(speed, start.z - landingZ);
    for (int round = 0; round < 3; ++round)
    {
        std::optional<float> const ahead = floorAt(start.x + std::cos(facing) * distance, start.y + std::sin(facing) * distance, start.z);
        if (!ahead)
        {
            break;
        }
        if (*ahead >= start.z - hover)
        {
            return std::nullopt;
        }
        landingZ = *ahead + hover;
        float const refined = OrbGlideDistance(speed, start.z - landingZ);
        bool const settled = std::fabs(refined - distance) < 0.1f;
        distance = refined;
        if (settled)
        {
            break;
        }
    }
    return OrbGlidePoint{ start.x + std::cos(facing) * distance, start.y + std::sin(facing) * distance, landingZ };
}

bool Mage::OrbOwnerActs(bool found, bool alive, bool inWorld, bool onOrbsMap)
{
    return found && alive && inWorld && onOrbsMap;
}

bool Mage::OrbEndDue(bool wasSnared, bool snaredNow, int32 lifeMs)
{
    return (wasSnared && !snaredNow) || lifeMs <= 0;
}

Mage::OrbSpells const* Mage::OrbSpellsFor(uint32 spellId)
{
    for (OrbSpells const& orb : ORBS)
    {
        if (spellId && (orb.castSpell == spellId || orb.summonSpell == spellId))
        {
            return &orb;
        }
    }
    return NULL;
}

bool Mage::IsOrbCreature(uint32 creatureEntry)
{
    for (OrbSpells const& orb : ORBS)
    {
        if (creatureEntry && orb.creature == creatureEntry)
        {
            return true;
        }
    }
    return false;
}

Unit* Mage::ClosestOrbTarget(WorldObject const& from, std::list<Unit*> const& candidates,
                             std::function<bool(Unit const&)> const& sees)
{
    Unit* closest = NULL;
    float closestDistance = 0.0f;
    for (Unit* unit : candidates)
    {
        if (!unit || !unit->IsAlive() || unit->GetCreatureType() == CREATURE_TYPE_CRITTER || unit->IsPolymorphed() ||
            !from.Where().ShareFrame(unit->Where()) || !sees(*unit))
        {
            continue;
        }
        float const distance = from.Where().DistanceTo(unit->Where());
        if (!closest || distance < closestDistance)
        {
            closest = unit;
            closestDistance = distance;
        }
    }
    return closest;
}

void Mage::KeepClosestOrbTarget(WorldObject const& from, std::list<Unit*>& targets,
                                std::function<bool(Unit const&)> const& sees)
{
    Unit* closest = ClosestOrbTarget(from, targets, sees);
    targets.clear();
    if (closest)
    {
        targets.push_back(closest);
    }
}

Mage::OrbCadence Mage::OrbCadenceFor(OrbSpells const& spells)
{
    OrbCadence cadence = { 0, 0 };
    SpellEntry const* aura = sSpellStore.LookupEntry(spells.orbAura);
    SpellEffectEntry const* tick = aura ? aura->GetSpellEffect(EFFECT_INDEX_0) : NULL;
    SpellEntry const* scan = sSpellStore.LookupEntry(spells.scanSpell);
    int32 const interval = scan ? scan->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
    int32 const snare = GetSpellDuration(sSpellStore.LookupEntry(spells.snareSpell));
    if (!tick || !tick->EffectAuraPeriod || interval <= 0 || snare <= 0)
    {
        return cadence;
    }
    uint32 const scansPerHit = uint32(interval) / tick->EffectAuraPeriod;
    cadence.scansPerHit = scansPerHit ? scansPerHit : 1;
    cadence.window = uint32(snare) / tick->EffectAuraPeriod;
    return cadence;
}

bool Mage::OrbHitDue(uint32 scan, OrbCadence const& cadence)
{
    return cadence.scansPerHit && scan < cadence.window && scan % cadence.scansPerHit == 0;
}

uint32 Mage::OrbExplosionChance(uint32 talentSpell)
{
    for (OrbSpells const& orb : ORBS)
    {
        for (uint32 talent : orb.explosionTalents)
        {
            if (talentSpell && talent == talentSpell)
            {
                SpellEntry const* spell = sSpellStore.LookupEntry(talentSpell);
                return spell ? spell->GetProcChance() : 0;
            }
        }
    }
    return 0;
}

int32 Mage::OrbCostShare(uint32 procSpellId, Unit const& mage)
{
    for (OrbSpells const& orb : ORBS)
    {
        if (!procSpellId || orb.hitSpell != procSpellId)
        {
            continue;
        }
        SpellEntry const* cast = sSpellStore.LookupEntry(orb.castSpell);
        OrbCadence const cadence = OrbCadenceFor(orb);
        uint32 const hits = cadence.scansPerHit ? (cadence.window + cadence.scansPerHit - 1) / cadence.scansPerHit : 0;
        if (!cast || !hits)
        {
            return 0;
        }
        int32 const cost = int32(cast->GetManaCost() + cast->GetManaCostPercentage() * mage.GetCreateMana() / 100);
        return cost / int32(hits);
    }
    return 0;
}

uint32 Mage::RegisterOrb(SpellHandlerRegistry& registry)
{
    CreatureAIRegistry& ais = CreatureAIRepository::Instance();
    if (!ais.HasItem(ORB_AI))
    {
        (new CreatureAIFactory<OrbAI>(ORB_AI))->RegisterSelf();
    }

    uint32 rows = 0;
    for (OrbSpells const& orb : ORBS)
    {
        RegisterScript<DummyEffectHook>(registry, orb.castSpell, &FlameOrbEffect);
        RegisterScript<SelectTargetsHook>(registry, orb.scanSpell, &OrbScanTargets);
        RegisterScript<DummyEffectHook>(registry, orb.scanSpell, &OrbScanEffect);
        rows += 3;
    }
    return rows;
}
