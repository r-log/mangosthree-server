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

#include "spells/classes/priest/shadow/ShadowyApparition.h"
#include "spells/data/SpellMgr.h"
#include "ai/CreatureAI.h"
#include "entities/Creature.h"
#include "entities/Pet.h"
#include "entities/Unit.h"
#include "Map.h"
#include "entities/motion/MotionMaster.h"
#include "movement/MoveSpline.h"
#include "DBCStores.h"
#include "Utilities/Util.h"

#include <cstdint>

namespace
{
    /// The talent's three ranks, the first rank first. Each holds two dummy effects; effect 0 holds the chance in
    /// percent of the summon (4, 8 and 12): "$s1% chance" in the client's text.
    uint32 const SHADOWY_APPARITION_RANKS[] = { 78202, 78203, 78204 };

    /// The summon: a spell whose one effect summons creature 46954 (summon properties 3062, a guardian) behind the
    /// caster for 20 s. Its effect 0 value, 4, is the creature count the engine reads and the most apparitions that
    /// stand ("up to $78202s3 Shadowy Apparitions active at a time").
    uint32 const SHADOWY_APPARITION_SUMMON = 87426;
    uint32 const SHADOWY_APPARITION_SUMMON_ROW[] = { SHADOWY_APPARITION_SUMMON };

    /// The apparition: creature 46954.
    uint32 const SHADOWY_APPARITION_CREATURE = 46954;

    /// The priest's look on the apparition: a mirror image (aura 247) the priest casts at it.
    uint32 const SHADOWY_APPARITION_LOOK = 87213;

    /// The apparition's visual: a dummy aura it casts on itself, which stays while it stands.
    uint32 const SHADOWY_APPARITION_VISUAL = 87427;

    /// The hit: Shadow damage at the coefficient 0.515 with a 10 s cooldown and the range of a melee hit.
    uint32 const SHADOWY_APPARITION_HIT = 87532;

    /// The tick that may summon: Shadow Word: Pain.
    uint32 const SHADOW_WORD_PAIN = 589;

    /// The apparitions one summon makes: one, whatever the effect's value (the engine reads it as a count).
    int32 const APPARITIONS_PER_SUMMON = 1;

    /// The text's "${$m1*5}": the chance is five times the rank's while the priest moves. The client's data holds it
    /// only in the text.
    int32 const MOVING_FACTOR = 5;

    /// The rolls of 0 to 99 a chance is held against.
    int32 const ROLL_RANGE = 100;

    /// The name the apparition's creature AI is registered under.
    char const* const APPARITION_AI = "ShadowyApparitionAI";

    /// The id of the leg the apparition walks to its unit.
    uint32 const APPARITION_LEG = 1;

    /// The distance in yards its unit may move from the point the apparition's leg goes to before the leg is laid again.
    float const APPARITION_DRIFT = 2.0f;

    /// The least time between two legs, in milliseconds.
    int32 const APPARITION_RELAY_MS = 500;

    typedef SpellHandlerOutcome<SpellAuraProcResult> ProcOutcome;

    /// The value of effect 0 of `spellId` in the client's data, for a rank of the talent or the summon only; 0 for any
    /// other spell and when the store lacks it.
    int32 EffectZero(uint32 spellId)
    {
        if (spellId != SHADOWY_APPARITION_SUMMON && spellId != SHADOWY_APPARITION_RANKS[0] &&
            spellId != SHADOWY_APPARITION_RANKS[1] && spellId != SHADOWY_APPARITION_RANKS[2])
        {
            return 0;
        }
        SpellEntry const* row = sSpellStore.LookupEntry(spellId);
        return row ? row->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
    }

    /// The apparition. On its first update an apparition that 87426 made takes its priest (its owner, who the summon
    /// made it for) as the unit that casts his look (87213) at it, and casts the visual (87427) on itself. Then, on
    /// every update, it goes (Priest::ShadowyApparitionGoesOn) when its unit does not stand or its priest is not
    /// alive on its map; strikes with 87532 when it is within Priest::ShadowyApparitionReach of its unit; and walks
    /// to its unit otherwise, at the creature's run speed, laying its leg again when the leg ended or its unit moved
    /// more than APPARITION_DRIFT yards from the leg's goal, at most once in APPARITION_RELAY_MS. Its unit is the unit
    /// it targets. An apparition no cast of 87426 made does nothing, and one that targets none goes. It attacks
    /// nothing and evades nothing.
    class ShadowyApparitionAI : public CreatureAI
    {
        public:
            explicit ShadowyApparitionAI(Creature* apparition) : CreatureAI(apparition), m_started(false),
                m_standing(false), m_laid(false), m_relayMs(0), m_goalX(0.0f), m_goalY(0.0f) {}

            void Reset() override {}

            /// Starts the apparition on its first update, when the summon has set the spell that created it; then
            /// goes, strikes or walks as the class comment says.
            void UpdateAI(uint32 const diff) override
            {
                if (!m_started)
                {
                    Start();
                }
                if (!m_standing)
                {
                    return;
                }
                Unit* owner = Owner();
                Unit* quarry = Quarry(owner);
                if (!Priest::ShadowyApparitionGoesOn(quarry != NULL, owner && owner->IsAlive()))
                {
                    End(owner);
                    return;
                }
                if (InReach(*m_creature, *quarry, Priest::ShadowyApparitionReach()))
                {
                    Strike(*owner, *quarry);
                    End(owner);
                    return;
                }
                Walk(*quarry, diff);
            }

            void AttackStart(Unit* /*who*/) override {}
            void AttackedBy(Unit* /*attacker*/) override {}
            void MoveInLineOfSight(Unit* /*who*/) override {}
            void EnterCombat(Unit* /*enemy*/) override {}
            void EnterEvadeMode() override {}

            /// One above the pet AI's permit for the apparition's creature, which every guardian answers, so that the
            /// permit and not the registry's order picks this AI; PERMIT_BASE_NO for any other creature.
            static int Permissible(Creature const* creature)
            {
                return Priest::IsShadowyApparitionCreature(creature->GetEntry()) ? PERMIT_BASE_SPECIAL + 1
                                                                                 : PERMIT_BASE_NO;
            }

        private:
            /// The apparition's priest: the unit of its owner's guid on its map, when he is in the world; NULL
            /// otherwise.
            Unit* Owner() const
            {
                Map* map = m_creature->FindMap();
                ObjectGuid const& owner = m_creature->GetOwnerGuid();
                Unit* unit = map && !owner.IsEmpty() ? map->GetUnit(owner) : NULL;
                return unit && unit->IsInWorld() ? unit : NULL;
            }

            /// The unit the apparition walks to, when it stands and the priest may attack it: the unit it targets
            /// (the one the cast of the summon was aimed at, ShadowyApparitionSummoned), found on its map by its guid;
            /// NULL for none, a unit that is dead and one friendly to the priest.
            Unit* Quarry(Unit* owner) const
            {
                Map* map = m_creature->FindMap();
                ObjectGuid const& target = m_creature->GetTargetGuid();
                Unit* unit = map && !target.IsEmpty() ? map->GetUnit(target) : NULL;
                if (!unit || !unit->IsInWorld() || !unit->IsAlive() || (owner && owner->IsFriendlyTo(unit)))
                {
                    return NULL;
                }
                return unit;
            }

            /// The start: for an apparition 87426 made and whose priest is on its map, its priest's look cast at it
            /// as he casts a mirror image, and its visual cast on itself. Any other apparition starts nothing and
            /// stays still.
            void Start()
            {
                m_started = true;
                if (m_creature->GetUInt32Value(UNIT_CREATED_BY_SPELL) != SHADOWY_APPARITION_SUMMON)
                {
                    return;
                }
                Unit* owner = Owner();
                if (!owner)
                {
                    return;
                }
                m_standing = true;
                owner->CastSpell(m_creature, SHADOWY_APPARITION_LOOK, true);
                m_creature->CastSpell(m_creature, SHADOWY_APPARITION_VISUAL, true);
            }

            /// The hit: 87532 at `quarry`, cast by the apparition triggered by its own visual aura, with `owner` as
            /// the original caster. The unit that deals it is the priest and it takes his bonuses; the cast triggered
            /// by an aura starts no proc of his hit. Without the visual it casts nothing, since a cast with no
            /// triggering aura would start his procs.
            void Strike(Unit& owner, Unit& quarry)
            {
                Aura* const visual = m_creature->GetAura(SHADOWY_APPARITION_VISUAL, EFFECT_INDEX_0);
                if (!visual)
                {
                    return;
                }
                m_creature->CastSpell(&quarry, SHADOWY_APPARITION_HIT, true, NULL, visual, owner.GetObjectGuid());
            }

            /// The walk: a leg to `quarry`'s place, laid when none is, when the last one has ended and when the unit
            /// has moved APPARITION_DRIFT yards from its goal, at most once in APPARITION_RELAY_MS.
            void Walk(Unit& quarry, uint32 diff)
            {
                m_relayMs = m_relayMs > int32(diff) ? m_relayMs - int32(diff) : 0;
                if (m_relayMs > 0)
                {
                    return;
                }
                float const goalX = quarry.Where().X() - m_goalX;
                float const goalY = quarry.Where().Y() - m_goalY;
                bool const walking = m_creature->movespline && !m_creature->movespline->Finalized();
                bool const drifted = goalX * goalX + goalY * goalY > APPARITION_DRIFT * APPARITION_DRIFT;
                if (m_laid && walking && !drifted)
                {
                    return;
                }
                m_creature->GetMotionMaster()->MovePoint(APPARITION_LEG, quarry.Where().X(), quarry.Where().Y(),
                                                         quarry.Where().Z(), true);
                m_laid = true;
                m_goalX = quarry.Where().X();
                m_goalY = quarry.Where().Y();
                m_relayMs = APPARITION_RELAY_MS;
            }

            /// The end: the apparition is taken away as the pet it is, by its priest (`owner`, NULL when he is not
            /// on its map).
            void End(Unit* owner)
            {
                m_standing = false;
                if (Pet* pet = ToPet(m_creature))
                {
                    pet->Unsummon(PET_SAVE_AS_DELETED, owner);
                }
                else
                {
                    m_creature->ForcedDespawn();
                }
            }

            bool m_started;         ///< the first update has run
            bool m_standing;        ///< an apparition 87426 made that has not gone
            bool m_laid;            ///< a leg has been laid
            int32 m_relayMs;        ///< the milliseconds before the next leg may be laid
            float m_goalX;          ///< where the last leg goes to
            float m_goalY;
    };

    /// 78202, 78203, 78204: Shadowy Apparition: its proc flags take the priest's periodic damage, and it has no
    /// spell_proc_event row, so the script's answer is the whole check: true for the damage tick of Shadow Word: Pain
    /// (Priest::ShadowyApparitionTakes), false for every other event, which no longer reaches the aura's effects.
    SpellHandlerOutcome<bool> ShadowyApparitionCheckProc(CheckProcContext& ctx)
    {
        uint32 const spellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        return SpellHandlerOutcome<bool>::Return(Priest::ShadowyApparitionTakes(spellId, ctx.procFlag, ctx.damage));
    }

    /// 78202, 78203, 78204: Shadowy Apparition: a damage tick of Shadow Word: Pain (the check-proc row has let only
    /// that through, and the script asks again) rolls the rank's chance, five times it while his client's movement
    /// word says he moves (Priest::ShadowyApparitionChanceWhile), and on a win, while fewer than the limit of his
    /// apparitions stand (Priest::ShadowyApparitionRoom), casts the summon (87426) at the unit the tick hit, triggered
    /// by the aura. CANT_TRIGGER for any other event; FAILED for an effect other than the aura's first (its second
    /// holds nothing the proc uses), for a tick with no victim and for a priest whose apparitions fill the limit; OK
    /// for the roll, won or not.
    ProcOutcome ShadowyApparitionProc(OnProcContext& ctx)
    {
        uint32 const spellId = ctx.procSpell ? ctx.procSpell->ID : 0;
        if (!Priest::ShadowyApparitionTakes(spellId, ctx.procFlag, ctx.damage))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_CANT_TRIGGER);
        }
        if (ctx.effIndex != EFFECT_INDEX_0 || !ctx.victim)
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        bool const moving = (ctx.caster.WireFlags() & uint32(movementFlagsMask)) != 0;
        int32 const chance = Priest::ShadowyApparitionChanceWhile(Priest::ShadowyApparitionChance(ctx.spell.ID),
                                                                  moving);
        if (!Priest::ShadowyApparitionRolls(chance, irand(0, ROLL_RANGE - 1)))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_OK);
        }
        if (!Priest::ShadowyApparitionRoom(Priest::ShadowyApparitionsStanding(ctx.caster),
                                           Priest::ShadowyApparitionLimit()))
        {
            return ProcOutcome::Return(SPELL_AURA_PROC_FAILED);
        }
        ctx.CastTriggered(*ctx.victim, SHADOWY_APPARITION_SUMMON);
        return ProcOutcome::Return(SPELL_AURA_PROC_OK);
    }

    /// 87426: the summon, a cast by any unit: refused, silently, while as many apparitions of the caster's stand as
    /// the limit (Priest::ShadowyApparitionRoom). The cast the proc makes meets the same row.
    SpellHandlerOutcome<SpellCastResult> ShadowyApparitionSummonCheck(CheckCastContext& ctx)
    {
        if (Priest::ShadowyApparitionRoom(Priest::ShadowyApparitionsStanding(ctx.caster),
                                          Priest::ShadowyApparitionLimit()))
        {
            return SpellHandlerOutcome<SpellCastResult>::Continue();
        }
        return SpellHandlerOutcome<SpellCastResult>::Return(SPELL_FAILED_DONT_REPORT);
    }

    /// 87426: the summon's effect value, which the engine reads as the count of creatures (4 in the client's data):
    /// it becomes one (Priest::ShadowyApparitionSummonValue), so that each cast makes one apparition. Any other
    /// effect keeps its value.
    SpellHandlerOutcome<void> ShadowyApparitionSummonCount(EffectValueContext& ctx)
    {
        SpellEffectEntry const* effect = ctx.spell.GetSpellEffect(ctx.effIndex);
        ctx.value = Priest::ShadowyApparitionSummonValue(effect ? effect->Effect : 0, ctx.value);
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 87426: the summon, once the cast is over and the apparition stands: the cast's unit target, when it is a unit
    /// other than the caster, is the unit the caster's apparitions that target none yet walk to: each of them targets
    /// it (Unit::SetTargetGuid), which the apparition reads. One apparition targets none when a cast ends, the one
    /// the cast made.
    SpellHandlerOutcome<void> ShadowyApparitionSummoned(AfterCastContext& ctx)
    {
        if (ctx.target && ctx.target != &ctx.caster)
        {
            ObjectGuid const unit = ctx.target->GetObjectGuid();
            ctx.caster.CallForAllControlledUnits(
                [&unit](Unit* guardian)
                {
                    if (Priest::IsShadowyApparitionCreature(guardian->GetEntry()) &&
                        guardian->GetTargetGuid().IsEmpty())
                    {
                        guardian->SetTargetGuid(unit);
                    }
                },
                CONTROLLED_GUARDIANS);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

uint32 Priest::RegisterShadowyApparition(SpellHandlerRegistry& registry)
{
    CreatureAIRegistry& ais = CreatureAIRepository::Instance();
    if (!ais.HasItem(APPARITION_AI))
    {
        (new CreatureAIFactory<ShadowyApparitionAI>(APPARITION_AI))->RegisterSelf();
    }

    uint32 rows = RegisterRanks<CheckProcHook>(registry, SHADOWY_APPARITION_RANKS, &ShadowyApparitionCheckProc);
    rows += RegisterRanks<OnProcHook>(registry, SHADOWY_APPARITION_RANKS, &ShadowyApparitionProc);
    rows += RegisterRanks<CheckCastHook>(registry, SHADOWY_APPARITION_SUMMON_ROW, &ShadowyApparitionSummonCheck);
    rows += RegisterRanks<EffectValueHook>(registry, SHADOWY_APPARITION_SUMMON_ROW, &ShadowyApparitionSummonCount);
    rows += RegisterRanks<AfterCastHook>(registry, SHADOWY_APPARITION_SUMMON_ROW, &ShadowyApparitionSummoned);
    return rows;
}

bool Priest::IsShadowyApparitionCreature(uint32 creatureEntry)
{
    return creatureEntry && creatureEntry == SHADOWY_APPARITION_CREATURE;
}

int32 Priest::ShadowyApparitionChance(uint32 rank)
{
    if (rank == SHADOWY_APPARITION_SUMMON)
    {
        return 0;
    }
    return EffectZero(rank);
}

int32 Priest::ShadowyApparitionChanceWhile(int32 chance, bool moving)
{
    return moving ? chance * MOVING_FACTOR : chance;
}

bool Priest::ShadowyApparitionRolls(int32 chance, int32 roll)
{
    return chance > 0 && roll < chance;
}

bool Priest::ShadowyApparitionTakes(uint32 spellId, uint32 procFlag, uint32 damage)
{
    return (procFlag & PROC_FLAG_ON_DO_PERIODIC) && spellId == SHADOW_WORD_PAIN && damage > 0;
}

uint32 Priest::ShadowyApparitionLimit()
{
    int32 const limit = EffectZero(SHADOWY_APPARITION_SUMMON);
    return limit > 0 ? uint32(limit) : 0;
}

bool Priest::ShadowyApparitionRoom(uint32 standing, uint32 limit)
{
    return limit == 0 || standing < limit;
}

uint32 Priest::ShadowyApparitionsStanding(Unit const& priest)
{
    uint32 standing = 0;
    priest.CheckAllControlledUnits(
        [&standing](Unit const* guardian)
        {
            if (IsShadowyApparitionCreature(guardian->GetEntry()) && guardian->IsAlive())
            {
                ++standing;
            }
            return false;
        },
        CONTROLLED_GUARDIANS);
    return standing;
}

int32 Priest::ShadowyApparitionSummonValue(uint32 effect, int32 value)
{
    return effect == SPELL_EFFECT_SUMMON ? APPARITIONS_PER_SUMMON : value;
}

bool Priest::ShadowyApparitionGoesOn(bool target, bool priest)
{
    return target && priest;
}

float Priest::ShadowyApparitionReach()
{
    SpellEntry const* hit = sSpellStore.LookupEntry(SHADOWY_APPARITION_HIT);
    return hit ? GetSpellMaxRange(sSpellRangeStore.LookupEntry(hit->RangeIndex)) : 0.0f;
}
