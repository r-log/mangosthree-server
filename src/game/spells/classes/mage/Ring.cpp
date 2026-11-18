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
#include "DBCStores.h"

#include <set>

namespace
{
    /// The rings of the mage's spells, one row each: Ring of Frost, whose band starts 4.7 yards from its centre.
    Mage::RingSpells const RINGS[] =
    {
        { 82676, 44199, 82691, 91264, 4.7f },
    };

    /// The name the ring's creature AI is registered under.
    char const* const RING_AI = "RingAI";

    /// The effect of the cast spell that summons the ring.
    SpellEffectIndex const RING_SUMMON_EFFECT = EFFECT_INDEX_0;

    /// The effect of the cast spell whose aura on the mage ticks the ring; its script word on his holder holds the low
    /// guid of the ring that holder drives.
    SpellEffectIndex const RING_TICK_EFFECT = EFFECT_INDEX_1;

    /// The freeze's one effect.
    SpellEffectIndex const RING_FREEZE_EFFECT = EFFECT_INDEX_0;

    /// The row whose freeze is `spellId`; NULL for any other id.
    Mage::RingSpells const* RingOfFreeze(uint32 spellId)
    {
        for (Mage::RingSpells const& ring : RINGS)
        {
            if (spellId && ring.freezeSpell == spellId)
            {
                return &ring;
            }
        }
        return NULL;
    }

    /// The ring's summoner, looked up on the ring's own map by the guid of his summon, when he is in the world;
    /// NULL otherwise.
    Unit* RingOwner(Creature const& ring)
    {
        Map* map = ring.FindMap();
        Unit* owner = map ? map->GetUnit(ring.GetSummonerGuid()) : NULL;
        return owner && owner->IsInWorld() ? owner : NULL;
    }

    /// Whether `unit` is immune to `freeze` as the freeze's hit finds it (Unit::SpellHitResult and
    /// Spell::AddUnitTarget): immune to the spell, to damage of its school, or to its effect.
    bool RingImmune(Unit& unit, SpellEntry const& freeze)
    {
        return unit.IsImmuneToSpell(&freeze, false) || unit.IsImmunedToDamage(GetSpellSchoolMask(&freeze)) ||
               unit.IsImmuneToSpellEffect(&freeze, RING_FREEZE_EFFECT, false);
    }

    /// The ring of a mage, owned by him and of his level, as his orb is, so that its freeze takes the units he may
    /// attack and rolls against them by his level. On its first update it takes its row by the spell that created it,
    /// the cast spell's duration as its life, and writes its low guid into the script word of its owner's ring aura,
    /// which makes it the ring that aura drives. On every later update it counts its life down; it goes when the life
    /// has run out, setting that word back to 0 while it holds its guid, or when the word holds another value: his
    /// next cast's aura, whose word starts at 0 and then holds its own ring's guid. Of the targets of its freeze it
    /// keeps those it takes, and it remembers the units it showed Immune. It attacks nothing, moves nowhere and evades
    /// nothing.
    class RingAI : public CreatureAI
    {
        public:
            explicit RingAI(Creature* ring) : CreatureAI(ring), m_spells(NULL), m_lifeMs(0), m_started(false),
                m_claimed(false) {}

            void Reset() override {}

            /// Starts the ring on its first update, when the summon has set the spell that created it; then counts its
            /// life down and claims its owner's aura, or goes as the class comment says.
            void UpdateAI(uint32 const diff) override
            {
                if (!m_started)
                {
                    Start();
                }
                if (!m_spells)
                {
                    return;
                }
                Unit* owner = RingOwner(*m_creature);
                int32* word = owner ? ScriptWordOf(*owner, m_spells->castSpell, RING_TICK_EFFECT) : NULL;
                int32 const own = int32(m_creature->GetGUIDLow());
                m_lifeMs -= int32(diff);
                if (m_lifeMs <= 0)
                {
                    if (word && *word == own)
                    {
                        *word = 0;
                    }
                    Go();
                    return;
                }
                if (!word)
                {
                    return;
                }
                if (!m_claimed)
                {
                    *word = own;
                    m_claimed = true;
                }
                else if (*word != own)
                {
                    Go();
                }
            }

            /// For the freeze of its row: keeps the units the ring takes (Mage::RingTakeFor): in its band
            /// (Mage::RingInBand, by the distance between the centres, in the ring's frame) and holding neither the
            /// freeze nor the marker; a unit immune to the freeze is kept until the ring has shown it Immune, and is
            /// marked as shown when it sees the ring, as the freeze's target check takes sight (Spell::CheckTarget). A
            /// ring that went, or has no row, keeps none. The targets of any other spell stay.
            void FilterSpellTargets(SpellEntry const* spell, std::list<Unit*>& targets) override
            {
                if (!spell || !RingOfFreeze(spell->ID))
                {
                    return;
                }
                if (!m_spells || m_spells->freezeSpell != spell->ID)
                {
                    targets.clear();
                    return;
                }
                float const outer = Mage::RingOuterRadius(*m_spells);
                targets.remove_if([this, spell, outer](Unit* unit)
                {
                    if (!unit)
                    {
                        return true;
                    }
                    bool const inBand = m_creature->Where().ShareFrame(unit->Where()) &&
                                        Mage::RingInBand(m_creature->Where().DistanceTo(unit->Where()),
                                                         m_spells->innerRadius, outer);
                    bool const held = unit->HasAura(m_spells->freezeSpell) || unit->HasAura(m_spells->markerSpell);
                    bool const immune = inBand && !held && RingImmune(*unit, *spell);
                    switch (Mage::RingTakeFor(inBand, held, immune, m_shownImmune.count(unit->GetObjectGuid()) != 0))
                    {
                        case Mage::RING_FREEZE:
                            return false;
                        case Mage::RING_SHOW_IMMUNE:
                            if (HasLineOfSight(*unit, *m_creature))
                            {
                                m_shownImmune.insert(unit->GetObjectGuid());
                            }
                            return false;
                        default:
                            return true;
                    }
                });
            }

            void AttackStart(Unit* /*who*/) override {}
            void MoveInLineOfSight(Unit* /*who*/) override {}
            void EnterCombat(Unit* /*enemy*/) override {}
            void EnterEvadeMode() override {}

            /// PERMIT_BASE_SPECIAL for a creature of the ring table, PERMIT_BASE_NO for any other.
            static int Permissible(Creature const* creature)
            {
                return Mage::IsRingCreature(creature->GetEntry()) ? PERMIT_BASE_SPECIAL : PERMIT_BASE_NO;
            }

        private:
            /// The ring's start: its row, by the spell that created it; its summoner as its owner and his level as
            /// its own; the hit he would have on its rolls; its life, the cast spell's duration. A ring of no row does
            /// nothing.
            void Start()
            {
                m_started = true;
                m_spells = Mage::RingSpellsFor(m_creature->GetUInt32Value(UNIT_CREATED_BY_SPELL));
                if (!m_spells)
                {
                    return;
                }
                m_creature->SetOwnerGuid(m_creature->GetSummonerGuid());
                if (Unit* owner = RingOwner(*m_creature))
                {
                    m_creature->SetLevel(owner->getLevel());
                }
                Combat::TakeOwnerHitSnapshot(*m_creature);
                m_lifeMs = GetSpellDuration(sSpellStore.LookupEntry(m_spells->castSpell));
            }

            /// The ring's end: it keeps no row, so its freeze takes nobody, and it despawns.
            void Go()
            {
                m_spells = NULL;
                m_creature->ForcedDespawn();
            }

            Mage::RingSpells const* m_spells;   ///< the ring's row; NULL until its start and after it went
            int32 m_lifeMs;                     ///< the milliseconds the ring has left
            bool m_started;                     ///< the first update has run
            bool m_claimed;                     ///< the ring wrote its guid into its owner's aura
            std::set<ObjectGuid> m_shownImmune; ///< the units its freeze showed Immune
    };

    /// 82676: the mage's ring aura, on each tick of its periodic trigger, which names no trigger spell: when the ring
    /// whose low guid the holder's script word holds is his, alive and on his map, and has formed (Mage::RingFormed:
    /// the aura has run Mage::RingFormingMs), the ring casts its freeze (82691) at its own centre, triggered, with him
    /// as the original caster. Return() ends the tick: nothing else is cast and nothing is logged.
    SpellHandlerOutcome<void> RingAuraTick(PeriodicTickContext& ctx)
    {
        Mage::RingSpells const* spells = Mage::RingSpellsFor(ctx.spell.ID);
        if (!spells)
        {
            return SpellHandlerOutcome<void>::Continue();
        }
        int32 const ringLow = ctx.aura.ScriptWord(RING_TICK_EFFECT);
        Map* map = ctx.target.FindMap();
        Creature* ring = ringLow && map
                         ? map->GetCreature(ObjectGuid(HIGHGUID_UNIT, spells->creature, uint32(ringLow)))
                         : NULL;
        if (!ring || !ring->IsAlive() || ring->GetSummonerGuid() != ctx.target.GetObjectGuid())
        {
            return SpellHandlerOutcome<void>::Return();
        }
        int32 const elapsed = GetSpellDuration(&ctx.spell) - ctx.aura.Duration();
        if (Mage::RingFormed(elapsed, Mage::RingFormingMs(*spells)))
        {
            ring->CastSpell(ring->Where().X(), ring->Where().Y(), ring->Where().Z(), spells->freezeSpell, true, NULL,
                            NULL, ctx.target.GetObjectGuid());
        }
        return SpellHandlerOutcome<void>::Return();
    }

    /// 82691: the ring's freeze, at the targets of its effect: a ring that casts it, found on its map by its guid,
    /// keeps the units it takes (RingAI::FilterSpellTargets). A freeze no ring cast keeps its list.
    SpellHandlerOutcome<void> RingFreezeTargets(SelectTargetsContext& ctx)
    {
        Map* map = ctx.caster.FindMap();
        Creature* ring = map && Mage::IsRingCreature(ctx.caster.GetEntry())
                         ? map->GetCreature(ctx.caster.GetObjectGuid())
                         : NULL;
        if (ring && ring->AI() && ctx.effIndex == RING_FREEZE_EFFECT)
        {
            ring->AI()->FilterSpellTargets(&ctx.spell, ctx.targets);
        }
        return SpellHandlerOutcome<void>::Continue();
    }

    /// 82691: the ring's freeze, at its removal: a freeze removed by a dispel (Mage::RingMarkerOnRemove) leaves the
    /// marker (91264) on its unit, which the unit casts at itself for the freeze's caster. The removal's boosts run.
    SpellHandlerOutcome<void> RingFreezeAuraRemove(AuraRemoveContext& ctx)
    {
        Mage::RingSpells const* spells = RingOfFreeze(ctx.spell.ID);
        if (spells && Mage::RingMarkerOnRemove(ctx.mode))
        {
            ctx.CastTriggered(ctx.target, spells->markerSpell);
        }
        return SpellHandlerOutcome<void>::Continue();
    }
}

Mage::RingSpells const* Mage::RingSpellsFor(uint32 spellId)
{
    for (RingSpells const& ring : RINGS)
    {
        if (spellId && ring.castSpell == spellId)
        {
            return &ring;
        }
    }
    return NULL;
}

bool Mage::IsRingCreature(uint32 creatureEntry)
{
    for (RingSpells const& ring : RINGS)
    {
        if (creatureEntry && ring.creature == creatureEntry)
        {
            return true;
        }
    }
    return false;
}

bool Mage::RingInBand(float distance, float inner, float outer)
{
    return distance >= inner && distance <= outer;
}

int32 Mage::RingFormingMs(RingSpells const& spells)
{
    SpellEntry const* marker = sSpellStore.LookupEntry(spells.markerSpell);
    return marker ? GetSpellDuration(marker) : 0;
}

bool Mage::RingFormed(int32 elapsedMs, int32 formingMs)
{
    return formingMs > 0 && elapsedMs >= formingMs;
}

float Mage::RingOuterRadius(RingSpells const& spells)
{
    SpellEntry const* cast = sSpellStore.LookupEntry(spells.castSpell);
    SpellEffectEntry const* summon = cast ? cast->GetSpellEffect(RING_SUMMON_EFFECT) : NULL;
    return summon ? GetSpellRadius(sSpellRadiusStore.LookupEntry(summon->GetRadiusIndex())) : 0.0f;
}

Mage::RingTake Mage::RingTakeFor(bool inBand, bool held, bool immune, bool shownImmune)
{
    if (!inBand || held)
    {
        return RING_SKIP;
    }
    if (immune)
    {
        return shownImmune ? RING_SKIP : RING_SHOW_IMMUNE;
    }
    return RING_FREEZE;
}

bool Mage::RingMarkerOnRemove(AuraRemoveMode mode)
{
    return mode == AURA_REMOVE_BY_DISPEL;
}

uint32 Mage::RegisterRing(SpellHandlerRegistry& registry)
{
    CreatureAIRegistry& ais = CreatureAIRepository::Instance();
    if (!ais.HasItem(RING_AI))
    {
        (new CreatureAIFactory<RingAI>(RING_AI))->RegisterSelf();
    }

    uint32 rows = 0;
    for (RingSpells const& ring : RINGS)
    {
        RegisterScript<PeriodicTickHook>(registry, ring.castSpell, &RingAuraTick);
        RegisterScript<SelectTargetsHook>(registry, ring.freezeSpell, &RingFreezeTargets);
        RegisterScript<AuraRemoveHook>(registry, ring.freezeSpell, &RingFreezeAuraRemove);
        rows += 3;
    }
    return rows;
}
