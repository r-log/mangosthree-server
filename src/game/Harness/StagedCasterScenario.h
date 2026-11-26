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

#ifndef MANGOS_HARNESS_STAGEDCASTERSCENARIO_H
#define MANGOS_HARNESS_STAGEDCASTERSCENARIO_H

#include "SpellScenario.h"

#include <string>
#include <utility>
#include <vector>

/**
 * @file StagedCasterScenario.h
 * @brief StagedCasterScenario, the base of every scenario that stages a caster of a class against a target and runs
 * steps at it. The mage's scenarios derive from it through MageProcScenario (ScenariosSpell.cpp); a class's scenario
 * file, Harness/Scenarios<Class><Topic>.cpp, derives from it with the class it stages.
 */

namespace Harness
{
    /**
     * What every scenario derived from this class shares, directly or through a base of its own: a human of the class
     * the scenario names (`classId`; the mage scenarios' MageProcScenario names CLASS_MAGE) staged to level 85 with the
     * scenario's talents, glyphs, spell power, ratings and spells (Scenario::StagePlayer), then whatever the scenario
     * does to him once staged
     * (AfterStage), then the pet the scenario names (PetEntry), with its own AI when the scenario keeps it
     * (PetRunsAI) and the spells a summoned pet learns for its level when the scenario asks (PetLearnsSpells), and a
     * Camel (50249, or the creature TargetEntry names) staged to level TargetLevel() (85 unless the scenario sets its
     * own) ten yards in front of him (Scenario::SpawnTarget), as 950 stages them; then the scenario's steps, in order:
     *  - a cast: for a scenario that names an aura for its caster (CasterAura: one its spell asks for on its caster, or
     *    a hit aura), that aura put on him when he does not hold it, by a triggered self-cast or, when CasterAuraPut()
     *    is set, without a cast; his mana set to its maximum by call; for a step that must crit, Increased Critical
     *    Spell (23434, a spell crit chance aura) put on him at kCritAuraPoints by a triggered self-cast, and for one
     *    that must not, taken off him by call, or put on him at NonCritAuraPoints() when the scenario names such points
     *    (a holder of 23434 at other points is taken off first); then the spell cast at the Camel through the
     *    CAST_SPELL handler's calls. One every CastEveryMs() (kCastEveryMs unless the scenario sets its own), read
     *    CastReadMs() after (kCastReadMs unless the scenario sets its own). An aura cast is the same cast of a spell
     *    that applies an aura and sends no damage log of its own.
     *    A moving cast (MovingCast) is made with his client's movement word at MOVEFLAG_FORWARD from just before the
     *    cast to its read, where the word he had is put back; a cast that clears its cooldown first takes the spell's
     *    cooldown off him by call (Player::RemoveSpellCooldown), since the stepped clock never runs out a cooldown
     *    that ends on time(NULL); a cast the Camel strikes (CastStruckAt) has the Camel's swing at him the step's
     *    `swingAtMs` after the cast starts, inside the cast's windows, his health set as for a swing first. A cast
     *    without the caster aura (`noCasterAura`) takes the scenario's caster aura off him by call instead of putting it
     *    on; a cast as the client (`asClient`) is sent as the client's CMSG_CAST_SPELL to the CAST_SPELL handler itself
     *    (CastAsClient), so the spell the server casts is the one the handler chooses, and the record reads the step's
     *    cast from the packets that carry its cast count;
     *  - a pet cast (PetCast): the step's spell cast by the pet the scenario named;
     *  - a self aura: the step's spell cast by him at himself, triggered, with nothing put on him first;
     *  - a swing: his health set by call to the step's thousandths of his maximum when it names them (at least 1), else
     *    to SwingHealth() (his maximum unless the scenario names one), then the Camel's melee swing at him
     *    (Unit::AttackerStateUpdate). One every kSwingEveryMs, read kSwingReadMs after;
     *  - a blow: his health set as for a swing, then the Camel strikes him for the step's amount of the step's school
     *    through the calls the `.damage <amount> <school>` command makes (his CalculateDamageAbsorbAndResist, then,
     *    when anything is left, the Camel's DealDamageMods, DealDamage and SendAttackStateUpdate); his health is noted
     *    right after it. Read kSwingReadMs after; a blow that killed him has him resurrected at full health there, so
     *    the steps after it run;
     *  - his health set to its maximum by call, read kSwingReadMs after;
     *  - a kill: he deals the Camel its health as plain damage (Unit::DealDamage, no spell), read kSwingReadMs after;
     *    a killing cast (KillingCast) is a cast whose hit kills the Camel, its health set below the hit by an earlier
     *    step, and the achievement plan counts that kill as it counts a kill step's;
     *  - a removal: the holder of the step's spell taken off the Camel, by dispel
     *    (Unit::RemoveAuraHolderDueToSpellByDispel, one stack) or by the plain call (Unit::RemoveAurasDueToSpell),
     *    read kSwingReadMs after;
     *  - a Camel health step (TargetHealth): the Camel's health set by call to the step's percent of its maximum, read
     *    kSwingReadMs after;
     *  - a wait: nothing done, read kSwingReadMs after;
     *  - a cancel: the holder of the step's spell on him removed through RemoveAurasDueToSpellByCancel, as the client's
     *    right-click does, read kSwingReadMs after.
     * A step that names its own time to the next step takes it instead. A step is not run when a holder on either
     * unit that a proc can reach is not one the guard fingerprinted. Each step's rolls are drawn in map updates under
     * the map hook's RNG::Seed(TickSeed(base, order, elapsed)), so every run draws the same ones. Right after a step
     * acts and at its read, the scenario's own reads (ReadOwn) note what it keeps of the step in Seen::reads. At the
     * stage read, ReadModifiers notes each spell modifier read the scenario lists (Modifiers) and keeps its value.
     *
     * A scenario with several Camels (TargetCount) spawns them in a line across his front ten yards ahead,
     * kTargetSpacing apart, the first of them the recorder's `target`, and each step names its Camel. Around a cast it
     * marks his holders of the cast's spell on the other Camels in use and reads after the cast which of them the cast
     * removed, and with which remove mode; each read notes the watched holder on every Camel; the `steps` category
     * writes an aura cast's Camel after `@`. A scenario may spawn more Camels at offsets of its own from the first
     * (ExtraTargets), after those.
     *
     * Categories: `template`; `staged` the level, the free talent points against the level's, the primary tree, each
     * talent's rank spell known, the glyph slots, and for a scenario that stages glyphs or ratings each glyph's aura
     * and the two ratings with the haste and mastery they give; its text also names his cast-speed and cast-haste
     * fields; `steps` each step as the record reads it (a cast's spell, `>` and the spell the server cast when a cast
     * as the client cast another, `c` added when its damage log is a crit,
     * `miss` when SPELL_GO lists the Camel as missed, `none` when no damage log came; an aura cast's spell, `miss`
     * added as for a cast; a swing's `hit` when his health fell, else `swing`; a blow's `blow`; a health step's
     * `full`; a Camel health step's `health` and its percent; a kill's `kill`; a removal's `dispel` or `remove`), a
     * BUG when a step was refused or not read, a cast's crit flag is not the step's or an aura cast missed, but not
     * for a cast whose refusal the step wants (`refusal`), which the scenario's own category judges, nor for the crit
     * flag of a cast that may crit or not (`critAllowed`), which it judges too;
     * `triggered` the spells of the SPELL_GO packets in each step's windows other than the step's own, the crit
     * aura's and the caster aura's, which are what his procs and the auras cast; the scenario's own category;
     * `noPersistence`; `digest`.
     */
    class StagedCasterScenario : public SpellScenario
    {
    public:
        /// `classId` is the class of the human the scenario stages (MageProcScenario's is CLASS_MAGE).
        StagedCasterScenario(char const* name, int order, char const* ownCategory, uint8 classId)
            : SpellScenario(name, order, { "template", "staged", "steps", "triggered", ownCategory, "noPersistence",
                                           "digest" }),
              m_classId(classId) {}

        void Prepare() override
        {
            const uint32 level = 85;
            const std::vector<Step> steps = Steps();
            const std::vector<std::pair<uint32, uint32> > talents = Talents();
            const std::vector<uint32> spells = Spells();
            const std::vector<SpellPrint> prints = Prints();

            QuestPlan plan;
            plan.quest = 0;
            plan.classId = m_classId;
            plan.level = level;
            plan.dealsDamage = true;
            plan.takesDamage = TakesDamage();
            plan.heals = Heals();
            plan.dies = Dies();
            for (size_t i = 0; i < steps.size(); ++i)
            {
                plan.takesDamage = plan.takesDamage || steps[i].swing || steps[i].blow || steps[i].swingAtMs;
                if (steps[i].kill || steps[i].killing)
                {
                    ++plan.kills[TargetEntry()];
                }
                if (steps[i].item)
                {
                    ++plan.items[steps[i].item];
                }
            }
            for (size_t i = 0; i < prints.size(); ++i)
            {
                if (!prints[i].fields.count("absent"))
                {
                    plan.casts.insert(prints[i].spell);
                }
            }
            for (size_t i = 0; i < talents.size(); ++i)
            {
                TalentEntry const* talent = sTalentStore.LookupEntry(talents[i].first);
                if (!talent || talents[i].second >= MAX_TALENT_RANK || !talent->SpellRank[talents[i].second])
                {
                    Verdict(Invalid("talent " + U(talents[i].first) + " has no rank index " + U(talents[i].second) + " in Talent.dbc"));
                    return;
                }
                plan.spells.insert(talent->SpellRank[talents[i].second]);
                if (i == 0)
                {
                    // the first talent names the primary tree, and LearnTalent teaches its mastery and primary spells
                    if (std::vector<uint32> const* tree = GetTalentTreeMasterySpells(talent->TabID))
                    {
                        plan.spells.insert(tree->begin(), tree->end());
                    }
                    if (std::vector<uint32> const* tree = GetTalentTreePrimarySpells(talent->TabID))
                    {
                        plan.spells.insert(tree->begin(), tree->end());
                    }
                }
            }
            plan.spells.insert(spells.begin(), spells.end());
            const std::vector<uint32> later = SpellsLearnedLater();
            plan.spells.insert(later.begin(), later.end());
            std::string templateOk;
            QuestPreCheck pre;
            std::vector<uint32> actors = { TargetEntry() };
            const std::vector<std::pair<float, float> > extra = ExtraTargets();
            for (size_t camel = 0; camel < extra.size(); ++camel)
            {
                const uint32 entry = ExtraTarget(camel).first;
                if (std::find(actors.begin(), actors.end(), entry) == actors.end())
                {
                    actors.push_back(entry);
                }
            }
            if (!Qualify(prints, actors, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z) + PlayerLift(), PlayerFacing(), m_classId);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + TargetDistance();
            Creature* target = NULL;
            std::vector<ObjectGuid> targets;
            for (uint32 camel = 0; camel < TargetCount(); ++camel)
            {
                const float ty = P0.y + (float(camel) - float(TargetCount() - 1) / 2.0f) * kTargetSpacing;
                Creature* spawned = SpawnTarget(TargetEntry(), TargetLevel(), tx, ty, Ground(tx, ty, P0.z), 3.14159f);
                if (!spawned)
                {
                    Verdict(Invalid("the target (" + U(TargetEntry()) + ") did not spawn"));
                    return;
                }
                if (const uint32 targetAura = TargetAura())
                {
                    spawned->CastSpell(spawned, targetAura, true);
                }
                target = target ? target : spawned;
                targets.push_back(spawned->GetObjectGuid());
            }
            const float firstY = P0.y - float(TargetCount() - 1) / 2.0f * kTargetSpacing;
            for (size_t camel = 0; camel < extra.size(); ++camel)
            {
                const float ex = tx + extra[camel].first;
                const float ey = firstY + extra[camel].second;
                const std::pair<uint32, uint32> kind = ExtraTarget(camel);
                Creature* spawned = SpawnTarget(kind.first, kind.second, ex, ey, Ground(ex, ey, P0.z), 3.14159f);
                if (!spawned)
                {
                    Verdict(Invalid("the target (" + U(kind.first) + ") did not spawn"));
                    return;
                }
                if (const uint32 targetAura = TargetAura())
                {
                    spawned->CastSpell(spawned, targetAura, true);
                }
                targets.push_back(spawned->GetObjectGuid());
            }
            StartTimeSync(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->targets = targets;
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;
            st->steps = steps;
            st->seen.resize(steps.size());
            st->talents = talents;
            for (size_t i = 0; i < prints.size(); ++i)
            {
                st->printed.insert(prints[i].spell);
            }

            SpellWatch watch;
            watch.target = st->target;
            watch.combatLogFields = true;
            m_rec.Begin(Name(), p, watch);
            m_rec.Open("stage", false);           // a setup window: logged, never digested
            StagePlan stage;
            stage.level = level;
            stage.talents = talents;
            stage.glyphs = Glyphs();
            stage.hasteRating = HasteRating();
            stage.masteryRating = MasteryRating();
            stage.spellPower = SpellPower();
            stage.spells = spells;
            st->staged = StagePlayer(*p, stage);
            if (st->staged)
            {
                AfterStage(*p);
                if (const uint32 petEntry = PetEntry())
                {
                    Pet* pet = BuildOwnedPet(p, petEntry, P0.x, P0.y + kPetOffset, 0.0f, PetRunsAI());
                    if (!pet)
                    {
                        st->staged = false;
                    }
                    else if (PetLearnsSpells())
                    {
                        pet->InitLevelupSpellsForLevel();
                    }
                }
            }
            st->levelSet = p->getLevel();
            ReadStage(*st, p, target, level);
            ReadModifiers(*st, p);

            uint32 at = 300;
            for (uint32 k = 0; k < steps.size(); ++k)
            {
                At(at, [this, st, k]() { RunStep(*st, k); });
                if (steps[k].swingAtMs)
                {
                    At(at + steps[k].swingAtMs, [this, st, k]() { StrikeCast(*st, k); });
                }
                At(at + (IsCast(steps[k]) ? CastReadMs() : kSwingReadMs), [this, st, k]() { ReadStep(*st, k); });
                at += steps[k].everyMs ? steps[k].everyMs : steps[k].swing || steps[k].blow ? kSwingEveryMs : CastEveryMs();
            }
            At(at, [this, st]() { Finish(*st); });
        }

    protected:
        static constexpr uint32 kCamel = 50249;
        static constexpr uint32 kCritAura = 23434;
        static constexpr int32 kCritAuraPoints = 100;
        static constexpr uint32 kCastEveryMs = 4100;
        static constexpr uint32 kCastReadMs = 3600;
        static constexpr uint32 kSwingEveryMs = 600;
        static constexpr uint32 kSwingReadMs = 300;
        static constexpr float kTargetSpacing = 15.0f;
        static constexpr float kPetOffset = 4.0f;

        /// One step of a scenario: the Camel's swing at the mage, the mage's cast of `spell` at the Camel or at himself,
        /// the use of an item of his that casts `spell`, the removal of `spell`'s holder from the Camel, the Camel's blow
        /// of `amount` damage of `school` at the mage, the mage's health set to its maximum, the Camel's health set, the
        /// mage's kill of the Camel, a wait that only marks a moment for its reads, or the cancel of `spell`'s holder on
        /// the mage.
        struct Step
        {
            bool swing = false;
            uint32 spell = 0;
            bool crit = false;                  ///< a cast that must crit; one that must not when false
            bool critAllowed = false;           ///< a cast that may crit or not: the steps check leaves its crit to the own category
            bool aura = false;                  ///< a cast whose spell applies an aura and sends no damage log of its own
            bool remove = false;                ///< the removal of `spell`'s holder from the Camel
            AuraRemoveMode mode = AURA_REMOVE_BY_DEFAULT;   ///< a removal's mode: AURA_REMOVE_BY_DISPEL, or the plain call's
            uint32 everyMs = 0;                 ///< the time to the next step; 0 for the step kind's own
            uint32 target = 0;                  ///< the Camel the step acts on, by its index in St::targets
            bool blow = false;                  ///< the Camel's blow at the mage
            bool fullHealth = false;            ///< the mage's health set to its maximum
            uint32 amount = 0;                  ///< a blow's damage
            SpellSchoolMask school = SPELL_SCHOOL_MASK_NONE;    ///< a blow's school
            bool self = false;                  ///< an aura cast by him at himself, triggered, with nothing put on him first
            bool moving = false;                ///< a cast made with his client's movement word at MOVEFLAG_FORWARD
            bool clearCooldown = false;         ///< a cast whose spell's cooldown is taken off him by call first
            bool refusal = false;               ///< a cast whose refusal (an aura cast's miss) the step wants: the own category judges it
            uint32 swingAtMs = 0;               ///< a cast the Camel's swing strikes this long after it starts; 0 for none
            uint32 healthPct = 0;               ///< the Camel's health set to this percent of its maximum; 0 for no such step
            uint32 healthPermille = 0;          ///< a swing's: his health set to this many thousandths of his maximum
                                                ///< first, at least 1; 0 for SwingHealth()'s
            bool kill = false;                  ///< the mage deals the Camel its health as plain damage
            bool killing = false;               ///< a cast whose hit kills the Camel: the plan counts the kill
            bool wait = false;                  ///< nothing done: the step only marks a moment for its reads
            bool selfRemoval = false;           ///< the holder of `spell` on the mage cancelled as the client's right-click does
            bool noCasterAura = false;          ///< a cast that takes the scenario's caster aura off him instead of putting it on
            bool asClient = false;              ///< a cast sent as the client's CMSG_CAST_SPELL to the cast handler (CastAsClient)
            bool petCast = false;               ///< a cast by his pet at the Camel; its own category judges it
            uint32 item = 0;                    ///< an aura cast at himself as the use of his item of this entry (UseItemAsClient)
        };
        /// The aura of `spell` put on `unit` as the GM aura command puts it: a holder and one aura per aura effect,
        /// with no cast. `caster` is the holder's caster; `unit` itself when NULL.
        static void PutAura(Unit* unit, uint32 spell, Unit* caster = NULL)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spell);
            if (!info)
            {
                return;
            }
            SpellAuraHolder* holder = CreateSpellAuraHolder(info, unit, caster ? caster : unit);
            for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
            {
                SpellEffectEntry const* effect = info->GetSpellEffect(SpellEffectIndex(i));
                if (effect && effect->Effect < TOTAL_SPELL_EFFECTS &&
                    (IsAreaAuraEffect(effect->Effect) || effect->Effect == SPELL_EFFECT_APPLY_AURA || effect->Effect == SPELL_EFFECT_PERSISTENT_AREA_AURA))
                {
                    holder->AddAura(CreateAura(info, SpellEffectIndex(i), NULL, holder, unit, caster),
                                    SpellEffectIndex(i));
                }
            }
            unit->AddSpellAuraHolder(holder);
        }
        /// The aura of `spell` put on `unit` as PutAura puts it, for `durationMs`, and ready as a cast leaves it, so
        /// that its procs run.
        static void PutReadyAura(Unit* unit, uint32 spell, int32 durationMs, Unit* caster = NULL)
        {
            PutAura(unit, spell, caster);
            if (SpellAuraHolder* holder = unit->GetSpellAuraHolder(spell))
            {
                holder->SetAuraMaxDuration(durationMs);
                holder->SetAuraDuration(durationMs);
                holder->SetState(SPELLAURAHOLDER_STATE_READY);
            }
        }
        static Step Cast(uint32 spell, bool crit)
        {
            Step s;
            s.spell = spell;
            s.crit = crit;
            return s;
        }
        static Step Swing()
        {
            Step s;
            s.swing = true;
            return s;
        }
        /// A swing at him with his health set to `healthPermille` thousandths of his maximum first, `everyMs` before
        /// the next step.
        static Step SwingAt(uint32 healthPermille, uint32 everyMs)
        {
            Step s = Swing();
            s.healthPermille = healthPermille;
            s.everyMs = everyMs;
            return s;
        }
        /// A cast of `spell` by his pet at the Camel, `everyMs` before the next step.
        static Step PetCast(uint32 spell, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.petCast = true;
            s.everyMs = everyMs;
            return s;
        }
        static Step AuraCast(uint32 spell, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.aura = true;
            s.everyMs = everyMs;
            return s;
        }
        /// An aura cast at the Camel of index `camel`.
        static Step AuraCastAt(uint32 spell, uint32 camel, uint32 everyMs)
        {
            Step s = AuraCast(spell, everyMs);
            s.target = camel;
            return s;
        }
        static Step Removal(uint32 spell, AuraRemoveMode mode, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.remove = true;
            s.mode = mode;
            s.everyMs = everyMs;
            return s;
        }
        static Step Blow(uint32 amount, SpellSchoolMask school, uint32 everyMs)
        {
            Step s;
            s.blow = true;
            s.amount = amount;
            s.school = school;
            s.everyMs = everyMs;
            return s;
        }
        static Step FullHealth(uint32 everyMs)
        {
            Step s;
            s.fullHealth = true;
            s.everyMs = everyMs;
            return s;
        }
        /// A cast that must not crit, made while his client's movement word says he moves forward.
        static Step MovingCast(uint32 spell)
        {
            Step s = Cast(spell, false);
            s.moving = true;
            return s;
        }
        /// A cast that must not crit, struck by the Camel's swing at him `swingAtMs` after it starts.
        static Step CastStruckAt(uint32 spell, uint32 swingAtMs)
        {
            Step s = Cast(spell, false);
            s.swingAtMs = swingAtMs;
            return s;
        }
        /// An aura cast by him at himself; `everyMs` the time to the next step, 0 for a cast step's own.
        static Step SelfAura(uint32 spell, uint32 everyMs = 0)
        {
            Step s;
            s.spell = spell;
            s.aura = true;
            s.self = true;
            s.everyMs = everyMs;
            return s;
        }
        /// The use of his item `item`, whose use casts `spell`, an aura, at himself, as his client asks it; the scenario
        /// gives him the item before the step (GiveItem). `everyMs` the time to the next step, 0 for a cast step's own.
        static Step ItemUseStep(uint32 item, uint32 spell, uint32 everyMs = 0)
        {
            Step s = SelfAura(spell, everyMs);
            s.item = item;
            return s;
        }
        /// The Camel's health set to `pct` percent of its maximum.
        static Step TargetHealth(uint32 pct, uint32 everyMs)
        {
            Step s;
            s.healthPct = pct;
            s.everyMs = everyMs;
            return s;
        }
        /// Nothing done; the step's reads mark a moment, `everyMs` before the next step.
        static Step Wait(uint32 everyMs)
        {
            Step s;
            s.wait = true;
            s.everyMs = everyMs;
            return s;
        }
        /// The holder of `spell` on the mage cancelled through Unit::RemoveAurasDueToSpellByCancel.
        static Step SelfRemoval(uint32 spell, uint32 everyMs)
        {
            Step s;
            s.spell = spell;
            s.selfRemoval = true;
            s.everyMs = everyMs;
            return s;
        }
        /// The mage's kill of the Camel: he deals it its health as plain damage.
        static Step Kill(uint32 everyMs)
        {
            Step s;
            s.kill = true;
            s.everyMs = everyMs;
            return s;
        }
        /// A cast of `spell` at the Camel, which must crit when `crit` and not otherwise, whose hit kills the Camel:
        /// the scenario has set the Camel's health below the hit first.
        static Step KillingCast(uint32 spell, bool crit)
        {
            Step s = Cast(spell, crit);
            s.killing = true;
            return s;
        }
        /// Whether a step is a cast of the mage's, at the Camel or at himself: neither a swing, a removal, a blow, a
        /// health step, a kill, a wait nor a cancel.
        static bool IsCast(Step const& step)
        {
            return !step.swing && !step.remove && !step.blow && !step.fullHealth && !step.healthPct && !step.kill && !step.wait && !step.selfRemoval;
        }

        /// One spell modifier read: what the player's modifiers of `op` make of `base` for `spell`.
        struct ModifierRead
        {
            uint32 spell;
            SpellModOp op;
            float base;
        };

        /// What a step's two moments and its windows saw.
        struct Seen
        {
            bool ran = false, refused = false, read = false;
            uint32 castId = 0;
            uint32 cost = 0;                            ///< a cast's mana cost, Spell::CalculatePowerCost
            /// The Camel's for a cast and a kill, the mage's for a swing, a blow and a health step: before the step,
            /// and at its read, or for a blow right after it.
            uint32 healthBefore = 0, healthAfter = 0;
            bool targetFrozen = false;                  ///< Unit::IsFrozen on the Camel when the step ran
            std::string watchedBefore;                  ///< HolderOf the watched spell when the step ran
            std::string holder;                         ///< HolderOf the scenario's watched spell, at the read
            int32 watchedAmount = 0;                    ///< the watched holder's effect 0 amount at the read; 0 for none
            float castSpeed = 0.0f, castHaste = 0.0f;   ///< his cast-speed and cast-haste fields at the read
            bool periodicRead = false;                  ///< PeriodicOf answered for the watched holder, at the read
            int32 period = 0, timer = 0, duration = 0, maxDuration = 0;
            std::vector<std::string> holders;           ///< several Camels: HolderOf the watched spell on each, at the read
            std::string removed;                        ///< several Camels: what ReleaseOtherTargets answered
            bool watchedOnCooldown = false;             ///< the watched spell on cooldown on the mage, at the read
            bool logRead = false, crit = false, missed = false, goSeen = false;
            uint32 amount = 0;
            std::vector<uint32> triggered;
            uint32 wordBefore = 0;                      ///< a moving cast: his client's movement word before the step
            uint32 swingHealthBefore = 0, swingHealthAfter = 0;   ///< a cast the Camel strikes: his health around the swing
            std::map<std::string, int64> reads;         ///< what the scenario's own reads (ReadOwn) kept of the step
            int64 threatUpdate = -1;                    ///< the last SMSG_THREAT_UPDATE value for the mage in the windows; -1 for none
            /// A cast as the client: the spell of the first SMSG_SPELL_START or SMSG_SPELL_GO of the step's cast count in
            /// its windows, the spell the handler cast; 0 when neither came.
            uint32 castSpell = 0;
        };

        struct St
        {
            ObjectGuid player;
            ObjectGuid target;
            std::vector<ObjectGuid> targets;            ///< every Camel, `target` first
            std::string templateOk;
            uint32 levelSet = 0;
            bool noReachLevel = false;
            std::set<uint32> achievementsAtSpawn;
            bool dealsDamage = false;
            QuestPlan plan;
            bool staged = false;
            std::string stagedText;
            bool stagedOk = false;
            float stagedCastSpeed = 0.0f, stagedCastHaste = 0.0f;   ///< his cast-speed and cast-haste fields once staged
            std::set<uint32> printed;
            std::vector<std::pair<uint32, uint32> > talents;
            std::vector<Step> steps;
            std::vector<Seen> seen;
            float stagedFireCrit = 0.0f;                ///< his fire spell crit at the stage read
            std::vector<float> modifiers;               ///< the values of Modifiers(), in its order, at the stage read
        };

        /// The talents to stage, as (talent id, rank index), in the order LearnTalent takes them.
        virtual std::vector<std::pair<uint32, uint32> > Talents() const = 0;
        /// The spells to stage besides the talents.
        virtual std::vector<uint32> Spells() const { return std::vector<uint32>(); }
        /// The spells the scenario teaches him at its own steps, after the stage; the achievement plan counts them with
        /// the staged ones.
        virtual std::vector<uint32> SpellsLearnedLater() const { return std::vector<uint32>(); }
        /// The glyphs to stage, as GlyphProperties ids.
        virtual std::vector<uint32> Glyphs() const { return std::vector<uint32>(); }
        /// The haste rating to stage.
        virtual int32 HasteRating() const { return 0; }
        /// The mastery rating to stage.
        virtual int32 MasteryRating() const { return 0; }
        /// The spell power to stage.
        virtual int32 SpellPower() const { return 0; }
        /// Whether the mage can take damage besides the Camel's swings and blows.
        virtual bool TakesDamage() const { return false; }
        /// Whether the mage heals or is healed.
        virtual bool Heals() const { return false; }
        /// Whether the caster dies in the run.
        virtual bool Dies() const { return false; }
        /// His health set before a swing or a blow; 0 for his maximum.
        virtual uint32 SwingHealth() const { return 0; }
        /// What the scenario does to him right after he is staged, before the staged line is read.
        virtual void AfterStage(Player& /*p*/) {}
        virtual std::vector<Step> Steps() const = 0;
        /// Every spell the scenario reaches: the casts, the holders that can proc and what they cast, the crit aura.
        virtual std::vector<SpellPrint> Prints() const = 0;
        /// The spell whose holder each read records; 0 for none.
        virtual uint32 Watched() const { return 0; }
        /// Whether the watched holder is the Camel's rather than the mage's.
        virtual bool WatchedOnTarget() const { return false; }
        /// Whether each step notes, before it acts, the Camel's frozen state and the watched holder.
        virtual bool NotesBeforeStep() const { return false; }
        /// Whether each read also records the watched holder's periodic timing (PeriodicOf).
        virtual bool ReadsPeriodic() const { return false; }
        /// The aura put on him before a cast step when he does not hold it: one the step's spell asks for on its caster,
        /// or one that keeps the cast from missing; 0 for none.
        virtual uint32 CasterAura() const { return 0; }
        /// Whether the caster aura is put on him as the GM aura command puts it (PutAura), with no cast and so no area
        /// or other cast check; false for a triggered self-cast.
        virtual bool CasterAuraPut() const { return false; }
        /// The aura each Camel casts on itself at its spawn: one that keeps the casts of a unit other than him from
        /// missing it; 0 for none.
        virtual uint32 TargetAura() const { return 0; }
        /// The level the Camels are spawned at; 85 by default.
        virtual uint32 TargetLevel() const { return 85; }
        /// The creature entry the Camels are spawned from; the Camel (50249) by default.
        virtual uint32 TargetEntry() const { return kCamel; }
        /// The creature entry of a pet built for him after he is staged (Scenario::BuildOwnedPet), kPetOffset yards to
        /// his side; 0 for none.
        virtual uint32 PetEntry() const { return 0; }
        /// Whether the pet keeps its own AI (Scenario::BuildOwnedPet's keepAI), so it acts on its own; false by
        /// default.
        virtual bool PetRunsAI() const { return false; }
        /// Whether the pet learns the spells of its level as a summoned pet does (Pet::InitLevelupSpellsForLevel);
        /// false by default.
        virtual bool PetLearnsSpells() const { return false; }
        /// The yards above the ground the mage stands at; none by default.
        virtual float PlayerLift() const { return 0.0f; }
        /// The way the mage faces, in radians; +x by default.
        virtual float PlayerFacing() const { return 0.0f; }
        /// How far ahead of him on +x the first Camel stands, in yards; ten by default.
        virtual float TargetDistance() const { return 10.0f; }
        /// The time from one cast step to the next.
        virtual uint32 CastEveryMs() const { return kCastEveryMs; }
        /// The points of the crit aura a cast that must not crit puts on him; 0 takes the aura off him instead.
        virtual int32 NonCritAuraPoints() const { return 0; }
        /// The time from a cast step to its read.
        virtual uint32 CastReadMs() const { return kCastReadMs; }
        /// The Camels to spawn: one ten yards in front of him, or several in a line across his front ten yards ahead,
        /// kTargetSpacing apart.
        virtual uint32 TargetCount() const { return 1; }
        /// More Camels, each at its offset from the first Camel (ahead of him, then across his front), in St::targets
        /// after TargetCount's; none unless the scenario names some.
        virtual std::vector<std::pair<float, float> > ExtraTargets() const { return std::vector<std::pair<float, float> >(); }
        /// The creature entry and the level of the extra Camel `camel` (ExtraTargets' order, from 0); TargetEntry() at
        /// TargetLevel() unless the scenario names another.
        virtual std::pair<uint32, uint32> ExtraTarget(size_t /*camel*/) const
        {
            return std::make_pair(TargetEntry(), TargetLevel());
        }
        /// The spell modifier reads the stage read notes and keeps (ReadModifiers); none by default.
        virtual std::vector<ModifierRead> Modifiers() const { return std::vector<ModifierRead>(); }
        /// The scenario's own reads of step k, right after the step acts (`atRead` false) and at its read (`atRead`
        /// true): what it notes in the step's window and keeps in st.seen[k].reads. Nothing by default.
        virtual void ReadOwn(St& /*st*/, uint32 /*k*/, bool /*atRead*/, Player* /*p*/, Creature* /*target*/) {}
        /// The scenario's own category, from what the steps saw.
        virtual std::string Own(St const& st) const = 0;

        /// The windows of step k (0-based), in their order.
        static std::vector<std::string> StepWindows(uint32 k)
        {
            const std::string base = "step" + U(k + 1);
            return { base, base + "+tick", base + "-read", base + "-read+tick" };
        }

        /// The delays of the SMSG_SPELL_DELAYED packets about him in step k's windows, in order: each packet read as
        /// Spell::Delayed writes it (BuildSpellDelayedPacket: the caster's packed guid, then the delay as uint32).
        std::vector<uint32> CastDelays(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> delays;
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(windows[w], SMSG_SPELL_DELAYED);
                for (size_t i = 0; i < seen.size(); ++i)
                {
                    std::string text;
                    uint32 delay = 0;
                    if (!seen[i]->payload.empty() && Trace::DecodeSpellDelayed(&seen[i]->payload[0], seen[i]->payload.size(), roles, text) &&
                        sscanf(text.c_str(), "caster=self delay=%u", &delay) == 1)
                    {
                        delays.push_back(delay);
                    }
                }
            }
            return delays;
        }

        /// The result of the first SMSG_CAST_FAILED of `spell` in step k's windows; -1 when none came.
        int32 CastFailedResult(uint32 k, uint32 spell) const
        {
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(windows[w], SMSG_CAST_FAILED);
                for (size_t i = 0; i < seen.size(); ++i)
                {
                    std::string text;
                    uint32 failed = 0, result = 0;
                    if (!seen[i]->payload.empty() && Trace::DecodeCastFailed(&seen[i]->payload[0], seen[i]->payload.size(), text) &&
                        sscanf(text.c_str(), "spell=%u result=%u", &failed, &result) == 2 && failed == spell)
                    {
                        return int32(result);
                    }
                }
            }
            return -1;
        }

        /// The name of a cast result the mage scenarios read, else its number.
        static std::string CastResultName(int32 result)
        {
            switch (result)
            {
                case -1:                                return "none";
                case SPELL_FAILED_CASTER_AURASTATE:     return "CASTER_AURASTATE";
                case SPELL_FAILED_INTERRUPTED:          return "INTERRUPTED";
                case SPELL_FAILED_MOVING:               return "MOVING";
                case SPELL_FAILED_NOT_READY:            return "NOT_READY";
                case SPELL_FAILED_SPELL_IN_PROGRESS:    return "SPELL_IN_PROGRESS";
                default:                                return "result" + std::to_string(result);
            }
        }

        /// The amounts of the periodic damage ticks of `spell` he dealt in step k's windows, in order.
        std::vector<uint32> TickAmounts(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> amounts;
            const std::string tick = "tick self.tick." + U(spell) + "=";
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_PERIODICAURALOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i]->payload.empty())
                    {
                        continue;
                    }
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &logs[i]->payload[0],
                                                                             logs[i]->payload.size(), roles);
                    for (size_t f = 0; f < fields.size(); ++f)
                    {
                        uint32 amount = 0;
                        if (fields[f].compare(0, tick.size(), tick) == 0 && sscanf(fields[f].c_str() + tick.size(), "%u", &amount) == 1)
                        {
                            amounts.push_back(amount);
                        }
                    }
                }
            }
            return amounts;
        }

        /// The damage logs of `spell` he dealt in step k's windows, in order: each one's amount, `c` added for a crit.
        std::vector<std::string> HitAmounts(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<std::string> hits;
            const std::string hit = "hit self.hit." + U(spell) + "=";
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(windows[w], SMSG_SPELLNONMELEEDAMAGELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    if (logs[i]->payload.empty())
                    {
                        continue;
                    }
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                             logs[i]->payload.size(), roles);
                    uint32 amount = 0, crit = 0;
                    if (fields.size() == 1 && fields[0].compare(0, hit.size(), hit) == 0 &&
                        sscanf(fields[0].c_str() + hit.size(), "%u/%u", &amount, &crit) == 2)
                    {
                        hits.push_back(U(amount) + (crit ? "c" : ""));
                    }
                }
            }
            return hits;
        }

        /// The cast count of an SMSG_SPELL_START or SMSG_SPELL_GO payload: the byte after the two packed guids its writer
        /// puts first (BuildSpellStartPacket, BuildSpellGoPacket); -1 when the payload ends before it.
        static int32 CastCountOf(std::vector<uint8> const& payload)
        {
            size_t pos = 0;
            for (int guid = 0; guid < 2; ++guid)
            {
                if (pos >= payload.size())
                {
                    return -1;
                }
                const uint8 mask = payload[pos++];
                for (int bit = 0; bit < 8; ++bit)
                {
                    pos += size_t((mask >> bit) & 1u);
                }
            }
            return pos < payload.size() ? int32(payload[pos]) : -1;
        }

        /// What step k's windows hold of his cast with the step's cast count (k + 1): the spell of the first
        /// SMSG_SPELL_START, its cast time and the window it came in, the spell of the first SMSG_SPELL_GO and its window,
        /// and the spell and result of the first SMSG_CAST_FAILED; 0 and -1 where none came. A window is its index in
        /// StepWindows(k), 0 being the step's own, which closes when the step's calls return.
        struct CastPath
        {
            uint32 start = 0;
            uint32 castTime = 0;
            int32 startWindow = -1;
            uint32 go = 0;
            int32 goWindow = -1;
            uint32 failed = 0;
            int32 result = -1;
        };
        CastPath CastPathOf(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            CastPath path;
            const std::vector<std::string> windows = StepWindows(k);
            const uint16 casts[] = { uint16(SMSG_SPELL_START), uint16(SMSG_SPELL_GO) };
            for (size_t w = 0; w < windows.size(); ++w)
            {
                for (const uint16 opcode : casts)
                {
                    const bool go = opcode == SMSG_SPELL_GO;
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(windows[w], opcode);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        std::string text;
                        uint32 spell = 0;
                        if (seen[i]->payload.empty() || CastCountOf(seen[i]->payload) != int32(k + 1) ||
                            !Trace::DecodeSpellCast(go, &seen[i]->payload[0], seen[i]->payload.size(), roles, text) ||
                            sscanf(text.c_str(), "spell=%u", &spell) != 1)
                        {
                            continue;
                        }
                        if (go && !path.go)
                        {
                            path.go = spell;
                            path.goWindow = int32(w);
                        }
                        else if (!go && !path.start)
                        {
                            path.start = spell;
                            path.startWindow = int32(w);
                            const size_t at = text.find(" casttime=");
                            if (at != std::string::npos)
                            {
                                sscanf(text.c_str() + at, " casttime=%u", &path.castTime);
                            }
                        }
                    }
                }
                std::vector<Recorder::Seen const*> failures = m_rec.SeenIn(windows[w], SMSG_CAST_FAILED);
                for (size_t i = 0; i < failures.size() && !path.failed; ++i)
                {
                    std::string text;
                    uint32 spell = 0, result = 0;
                    if (!failures[i]->payload.empty() && uint32(failures[i]->payload[0]) == k + 1 &&
                        Trace::DecodeCastFailed(&failures[i]->payload[0], failures[i]->payload.size(), text) &&
                        sscanf(text.c_str(), "spell=%u result=%u", &spell, &result) == 2)
                    {
                        path.failed = spell;
                        path.result = int32(result);
                    }
                }
            }
            return path;
        }

        /// The spell step k cast as the record reads it: for a cast as the client, the spell the handler cast when its
        /// packets came; the step's spell otherwise.
        static uint32 CastSpellOf(Step const& step, Seen const& seen)
        {
            return step.asClient && seen.castSpell ? seen.castSpell : step.spell;
        }

        /// Whether `amount` lies in [low, high], one point either side allowed for the float product of the percent
        /// auras the engine multiplies before it truncates.
        static bool InBand(uint32 amount, uint32 low, uint32 high)
        {
            return amount + 1 >= low && amount <= high + 1;
        }

        /// The maximum duration of the holder of `spell` on `unit`, as HolderOf finds it; -1 when there is none.
        static int32 HolderDuration(Unit* unit, uint32 spell)
        {
            SpellAuraHolder* holder = unit ? unit->GetSpellAuraHolder(spell) : NULL;
            return holder ? holder->GetAuraMaxDuration() : -1;
        }

        /// The cast steps' 1-based numbers, comma-separated, for which `pick` answers true.
        template <class Pick>
        static std::string StepsWhere(St const& st, Pick pick)
        {
            std::string out;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                if (pick(k))
                {
                    out += (out.empty() ? "" : ",") + U(k + 1);
                }
            }
            return out.empty() ? std::string("none") : out;
        }

        static bool Triggered(Seen const& seen, uint32 spell)
        {
            return std::find(seen.triggered.begin(), seen.triggered.end(), spell) != seen.triggered.end();
        }

        /// The amounts of `spell`'s SMSG_SPELLENERGIZELOG packets in the windows of step `k` (0-based), joined by `+`;
        /// "" when there is none.
        std::string EnergizeAmounts(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::string amounts;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (const char* suffix : suffixes)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLENERGIZELOG);
                for (size_t i = 0; i < logs.size(); ++i)
                {
                    std::string log;
                    uint32 logSpell = 0, power = 0, amount = 0;
                    if (!logs[i]->payload.empty() &&
                        Trace::DecodeSpellEnergizeLog(&logs[i]->payload[0], logs[i]->payload.size(), roles, log) &&
                        log.find(" spell=") != std::string::npos &&
                        sscanf(log.c_str() + log.find(" spell="), " spell=%u power=%u amount=%u", &logSpell, &power, &amount) == 3 &&
                        logSpell == spell)
                    {
                        amounts += (amounts.empty() ? "" : "+") + U(amount);
                    }
                }
            }
            return amounts;
        }

        /// The Camel step k acts on.
        static ObjectGuid TargetOf(St const& st, uint32 k)
        {
            const uint32 camel = st.steps[k].target;
            return camel < st.targets.size() ? st.targets[camel] : st.target;
        }

        /// The recorder's roles with `target` naming the Camel of index `camel`, so a packet decoded with them names
        /// that Camel `target` and the others `other`.
        Trace::Roles RolesAt(St const& st, uint32 camel) const
        {
            Trace::Roles roles = m_rec.Roles();
            if (camel < st.targets.size())
            {
                roles.target = st.targets[camel].GetRawValue();
            }
            return roles;
        }

        static std::string RemoveModeName(AuraRemoveMode mode)
        {
            switch (mode)
            {
                case AURA_REMOVE_BY_DEFAULT:        return "default";
                case AURA_REMOVE_BY_STACK:          return "stack";
                case AURA_REMOVE_BY_CANCEL:         return "cancel";
                case AURA_REMOVE_BY_DISPEL:         return "dispel";
                case AURA_REMOVE_BY_DEATH:          return "death";
                case AURA_REMOVE_BY_DELETE:         return "delete";
                case AURA_REMOVE_BY_SHIELD_BREAK:   return "shieldBreak";
                case AURA_REMOVE_BY_EXPIRE:         return "expire";
                case AURA_REMOVE_BY_TRACKING:       return "tracking";
                case AURA_REMOVE_BY_TARGET_LIMIT:   return "targetLimit";
                case AURA_REMOVE_BY_DAMAGE:         return "damage";
            }
            return "mode" + U(uint32(mode));
        }

    private:
        /// With several Camels, before a cast step: his holders of the step's spell on the Camels other than the
        /// step's, each marked in use (SpellAuraHolder::SetInUse), so that one the cast removes stays in memory, its
        /// deletion deferred to its unit's next update, and its remove mode can be read after the cast.
        std::vector<std::pair<uint32, SpellAuraHolder*> > HoldOtherTargets(St const& st, uint32 k, Player* p) const
        {
            std::vector<std::pair<uint32, SpellAuraHolder*> > held;
            if (TargetCount() < 2)
            {
                return held;
            }
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                Creature* other = camel == st.steps[k].target ? NULL : Get(st.targets[camel]);
                if (SpellAuraHolder* holder = other ? other->GetSpellAuraHolder(st.steps[k].spell, p->GetObjectGuid()) : NULL)
                {
                    holder->SetInUse(true);
                    held.push_back(std::make_pair(camel, holder));
                }
            }
            return held;
        }

        /// Releases what HoldOtherTargets held, right after the cast: answers "<Camel>:<remove mode>" for each holder
        /// the cast removed, comma-separated, "" when it removed none, and notes every held one.
        std::string ReleaseOtherTargets(std::vector<std::pair<uint32, SpellAuraHolder*> > const& held, uint32 spell)
        {
            std::string removed, text;
            for (size_t i = 0; i < held.size(); ++i)
            {
                SpellAuraHolder* holder = held[i].second;
                const std::string camel = U(held[i].first + 1);
                if (holder->IsDeleted())
                {
                    removed += (removed.empty() ? "" : ",") + camel + ":" + RemoveModeName(holder->GetRemoveMode());
                    text += (text.empty() ? "" : "; ") + std::string("Camel ") + camel + " removed (" + RemoveModeName(holder->GetRemoveMode()) + ")";
                }
                else
                {
                    text += (text.empty() ? "" : "; ") + std::string("Camel ") + camel + " kept";
                }
                holder->SetInUse(false);
            }
            if (!held.empty())
            {
                m_rec.Note("his " + U(spell) + " on the other Camels after the cast: " + text);
            }
            return removed;
        }

        void RunStep(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(TargetOf(st, k));
            if (!p || !target || !st.staged)
            {
                return;
            }
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            const std::string name = "step" + U(k + 1);
            m_rec.Open(name);
            seen.ran = true;
            m_rec.Note("holders that can proc: self [" + ProcHolders(p) + "], target [" + ProcHolders(target) + "]");
            const std::string unprintedSelf = ProcHolders(p, st.printed);
            const std::string unprintedTarget = ProcHolders(target, st.printed);
            if (!unprintedSelf.empty() || !unprintedTarget.empty())
            {
                seen.refused = true;
                m_rec.Note("holders that can proc and are not fingerprinted: self [" + unprintedSelf + "], target [" + unprintedTarget +
                           "]; the step is not run");
                m_rec.Open(name + "+tick");
                return;
            }
            seen.targetFrozen = target->IsFrozen();
            if (const uint32 watched = Watched())
            {
                seen.watchedBefore = HolderOf(WatchedOnTarget() ? static_cast<Unit*>(target) : static_cast<Unit*>(p), watched);
            }
            if (NotesBeforeStep())
            {
                m_rec.Note(std::string("before the step: the Camel ") + (seen.targetFrozen ? "frozen" : "not frozen") +
                           (Watched() ? "; " + U(Watched()) + (WatchedOnTarget() ? " on the Camel: " : " on him: ") + seen.watchedBefore
                                      : std::string()));
            }
            if (step.remove)
            {
                seen.healthBefore = target->GetHealth();
                m_rec.Note("the Camel's " + U(step.spell) + ": " + HolderOf(target, step.spell) + "; target health " +
                           U(seen.healthBefore) + (step.mode == AURA_REMOVE_BY_DISPEL ? "; dispelled" : "; removed"));
                if (step.mode == AURA_REMOVE_BY_DISPEL)
                {
                    target->RemoveAuraHolderDueToSpellByDispel(step.spell, 1, p->GetObjectGuid(), target);
                }
                else
                {
                    target->RemoveAurasDueToSpell(step.spell);
                }
            }
            else if (step.swing)
            {
                const uint32 permille = uint32(uint64(p->GetMaxHealth()) * step.healthPermille / 1000);
                const uint32 named = SwingHealth() ? SwingHealth() : p->GetMaxHealth();
                p->SetHealth(step.healthPermille ? std::max<uint32>(permille, 1) : named);
                seen.healthBefore = p->GetHealth();
                m_rec.Note("his health " + U(seen.healthBefore) + "; the Camel swings at him");
                target->AttackerStateUpdate(p);
            }
            else if (step.blow)
            {
                p->SetHealth(SwingHealth() ? SwingHealth() : p->GetMaxHealth());
                seen.healthBefore = p->GetHealth();
                m_rec.Note("his health " + U(seen.healthBefore) + " of " + U(p->GetMaxHealth()) + "; the Camel strikes him for " +
                           U(step.amount) + " of school mask " + U(uint32(step.school)));
                uint32 absorb = 0;
                uint32 resist = 0;
                p->CalculateDamageAbsorbAndResist(target, step.school, SPELL_DIRECT_DAMAGE, step.amount, &absorb, &resist);
                if (step.amount > absorb + resist)
                {
                    uint32 left = step.amount - absorb - resist;
                    target->DealDamageMods(p, left, &absorb);
                    target->DealDamage(p, left, NULL, DIRECT_DAMAGE, step.school, NULL, false);
                    target->SendAttackStateUpdate(HITINFO_NORMALSWING2, p, step.school, left, absorb, resist, VICTIMSTATE_NORMAL, 0);
                }
                seen.healthAfter = p->GetHealth();
                m_rec.Note("absorbed " + U(absorb) + ", resisted " + U(resist) + "; his health right after the blow " + U(seen.healthAfter) +
                           (p->IsAlive() ? "" : ", dead"));
            }
            else if (step.fullHealth)
            {
                seen.healthBefore = p->GetHealth();
                p->SetHealth(p->GetMaxHealth());
                m_rec.Note("his health " + U(seen.healthBefore) + " set to his maximum " + U(p->GetHealth()));
            }
            else if (step.kill)
            {
                seen.healthBefore = target->GetHealth();
                m_rec.Note("the Camel's health " + U(seen.healthBefore) + "; he deals it that much as plain damage");
                p->DealDamage(target, target->GetHealth(), NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                m_rec.Note("the Camel's health right after " + U(target->GetHealth()) + (target->IsAlive() ? "" : ", dead"));
            }
            else if (step.wait)
            {
                m_rec.Note("nothing done: the step marks a moment");
            }
            else if (step.selfRemoval)
            {
                m_rec.Note("his " + U(step.spell) + ": " + HolderOf(p, step.spell) + "; cancelled as the client's right-click does");
                p->RemoveAurasDueToSpellByCancel(step.spell);
            }
            else if (step.healthPct)
            {
                seen.healthBefore = target->GetHealth();
                target->SetHealth(target->GetMaxHealth() * step.healthPct / 100);
                m_rec.Note("the Camel's health " + U(seen.healthBefore) + " set to " + U(step.healthPct) + "% of " + U(target->GetMaxHealth()) +
                           ": " + U(target->GetHealth()) + (target->HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT) ? ", under 35%" : ""));
            }
            else if (step.petCast)
            {
                Pet* pet = p->GetPet();
                if (!pet)
                {
                    seen.refused = true;
                    m_rec.Note("he has no pet; the step is not run");
                    m_rec.Open(name + "+tick");
                    return;
                }
                if (const uint32 casterAura = CasterAura())
                {
                    if (!p->HasAura(casterAura))
                    {
                        PutAura(p, casterAura);
                    }
                }
                pet->SetPower(POWER_MANA, pet->GetMaxPower(POWER_MANA));
                seen.healthBefore = target->GetHealth();
                char buf[224];
                snprintf(buf, sizeof(buf), "his pet (level %u) casts %u at the Camel, which it reads at level %u; his spell hit %.2f%%, the pet's own %.2f%%; target health %u",
                         pet->getLevel(), step.spell, target->GetLevelForTarget(pet), p->m_modSpellHitChance, pet->m_modSpellHitChance,
                         seen.healthBefore);
                m_rec.Note(buf);
                pet->CastSpell(target, step.spell, false);
                seen.castId = step.spell;
            }
            else if (step.item)
            {
                seen.healthBefore = target->GetHealth();
                m_rec.Note("he uses his " + U(step.item) + ", whose use casts " + U(step.spell) + "; mana " + U(p->GetPower(POWER_MANA)) +
                           " of " + U(p->GetMaxPower(POWER_MANA)));
                if (step.clearCooldown)
                {
                    p->RemoveSpellCooldown(step.spell, true);
                    m_rec.Note("his cooldown of " + U(step.spell) + " taken off by call; stored keys [" + CooldownKeys(p) + "]");
                }
                seen.castId = UseItemAsClient(p, step.item, step.spell, uint8(k + 1));
                if (!seen.castId)
                {
                    seen.refused = true;
                    m_rec.Note("the step is not run");
                    m_rec.Open(name + "+tick");
                    return;
                }
            }
            else if (step.self)
            {
                seen.healthBefore = target->GetHealth();
                char buf[128];
                snprintf(buf, sizeof(buf), "he casts %u at himself, triggered; spell crit %.2f%% (fire)", step.spell,
                         p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE));
                m_rec.Note(buf);
                if (step.clearCooldown)
                {
                    p->RemoveSpellCooldown(step.spell, true);
                    m_rec.Note("his cooldown of " + U(step.spell) + " taken off by call; stored keys [" + CooldownKeys(p) + "]");
                }
                p->CastSpell(p, step.spell, true);
                seen.castId = step.spell;
            }
            else
            {
                if (const uint32 casterAura = CasterAura())
                {
                    if (step.noCasterAura)
                    {
                        const bool held = p->HasAura(casterAura);
                        p->RemoveAurasDueToSpell(casterAura);
                        m_rec.Note("his " + U(casterAura) + " taken off by call (" + (held ? "he held it" : "he did not hold it") + ")");
                    }
                    else if (!p->HasAura(casterAura))
                    {
                        if (CasterAuraPut())
                        {
                            PutAura(p, casterAura);
                        }
                        else
                        {
                            p->CastSpell(p, casterAura, true);
                        }
                    }
                }
                int32 points = step.crit ? kCritAuraPoints : NonCritAuraPoints();
                Aura const* critAura = p->GetAura(kCritAura, EFFECT_INDEX_0);
                const bool critAuraHolds = critAura && points && critAura->GetModifier()->m_amount == points;
                if (critAura && !critAuraHolds)
                {
                    p->RemoveAurasDueToSpell(kCritAura);
                }
                if (points && !critAuraHolds)
                {
                    p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
                }
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                seen.healthBefore = target->GetHealth();
                SpellEntry const* info = sSpellStore.LookupEntry(step.spell);
                seen.cost = info ? Spell::CalculatePowerCost(info, p) : 0;
                char buf[192];
                snprintf(buf, sizeof(buf), "spell crit %.2f%% (fire), %.2f%% (frost); mana %u, cost %u; target health %u",
                         p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE),
                         p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FROST), p->GetPower(POWER_MANA), seen.cost,
                         seen.healthBefore);
                m_rec.Note(buf);
                if (step.clearCooldown)
                {
                    p->RemoveSpellCooldown(step.spell, true);
                    m_rec.Note("his cooldown of " + U(step.spell) + " taken off by call; stored keys [" + CooldownKeys(p) + "]");
                }
                if (step.moving)
                {
                    MovementInfo moving = p->ClientStatus();
                    seen.wordBefore = uint32(moving.GetMovementFlags());
                    moving.SetMovementFlags(MovementFlags(seen.wordBefore | MOVEFLAG_FORWARD));
                    p->AcceptClientStatus(moving);
                    m_rec.Note(std::string("his client's movement word forward: ") + (p->isMoving() ? "moving" : "not moving"));
                }
                const std::vector<std::pair<uint32, SpellAuraHolder*> > held = HoldOtherTargets(st, k, p);
                seen.castId = step.asClient ? CastAsClient(p, step.spell, target, uint8(k + 1)) : CastAsHandler(p, step.spell, target, uint8(k + 1));
                seen.removed = ReleaseOtherTargets(held, step.spell);
            }
            ReadOwn(st, k, false, p, target);
            m_rec.Open(name + "+tick");
        }

        /// The Camel's swing at him during the cast of step k (CastStruckAt): his health set as for a swing, then
        /// Unit::AttackerStateUpdate, inside the cast's windows.
        void StrikeCast(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(TargetOf(st, k));
            Seen& seen = st.seen[k];
            if (!p || !target || !seen.ran || seen.refused)
            {
                return;
            }
            p->SetHealth(SwingHealth() ? SwingHealth() : p->GetMaxHealth());
            seen.swingHealthBefore = p->GetHealth();
            m_rec.Note("his health " + U(seen.swingHealthBefore) + "; the Camel swings at him during his cast of " + U(st.steps[k].spell));
            target->AttackerStateUpdate(p);
            seen.swingHealthAfter = p->GetHealth();
            m_rec.Note("his health after the swing " + U(seen.swingHealthAfter));
        }

        void ReadStep(St& st, uint32 k)
        {
            Player* p = sPlayerRegistry.Find(st.player);
            Creature* target = Get(TargetOf(st, k));
            Seen& seen = st.seen[k];
            if (!p || !target || !seen.ran || seen.refused)
            {
                return;
            }
            const std::string name = "step" + U(k + 1) + "-read";
            m_rec.Open(name);
            seen.read = true;
            Step const& step = st.steps[k];
            if (step.blow && !p->IsAlive())
            {
                p->ResurrectPlayer(1.0f);
                m_rec.Note("the blow killed him; resurrected at full health, " + U(p->GetHealth()));
            }
            if (!step.blow)
            {
                seen.healthAfter = step.swing || step.fullHealth ? p->GetHealth() : target->GetHealth();
            }
            if (step.moving)
            {
                MovementInfo before = p->ClientStatus();
                before.SetMovementFlags(MovementFlags(seen.wordBefore));
                p->AcceptClientStatus(before);
                m_rec.Note(std::string("his client's movement word put back: ") + (p->isMoving() ? "moving" : "not moving"));
            }
            seen.watchedOnCooldown = Watched() && p->HasSpellCooldown(Watched());
            seen.castSpeed = p->GetFloatValue(UNIT_MOD_CAST_SPEED);
            seen.castHaste = p->GetFloatValue(UNIT_MOD_CAST_HASTE);
            std::string periodic;
            if (const uint32 watched = Watched())
            {
                Unit* holderUnit = WatchedOnTarget() ? static_cast<Unit*>(target) : static_cast<Unit*>(p);
                seen.holder = HolderOf(holderUnit, watched);
                Aura const* watchedAura = holderUnit->GetAura(watched, EFFECT_INDEX_0);
                seen.watchedAmount = watchedAura ? watchedAura->GetModifier()->m_amount : 0;
                if (ReadsPeriodic())
                {
                    seen.periodicRead = PeriodicOf(holderUnit, watched, seen.period, seen.timer, seen.duration, seen.maxDuration);
                    periodic = seen.periodicRead ? "; its periodic effect: period " + std::to_string(seen.period) + ", next tick in " +
                                                   std::to_string(seen.timer) + ", duration " + std::to_string(seen.duration) + "/" +
                                                   std::to_string(seen.maxDuration)
                                                 : std::string("; no periodic effect");
                }
            }
            m_rec.Note("his health " + U(p->GetHealth()) + ", mana " + U(p->GetPower(POWER_MANA)) + "; the Camel's health " +
                       U(target->GetHealth()) +
                       (Watched() ? "; " + U(Watched()) + (WatchedOnTarget() ? " on the Camel: " : " on him: ") + seen.holder : std::string()) +
                       periodic);
            if (TargetCount() > 1 && Watched())
            {
                std::string text;
                for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                {
                    seen.holders.push_back(HolderOf(Get(st.targets[camel]), Watched()));
                    text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + " " + seen.holders.back();
                }
                m_rec.Note(U(Watched()) + " on each Camel: " + text);
            }
            ReadOwn(st, k, true, p, target);
            m_rec.Open(name + "+tick");
        }

        /// The step's windows as the record holds them: the SPELL_GO packets, the damage log of the step's spell.
        void ReadSeen(St& st) const
        {
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Step const& step = st.steps[k];
                Seen& seen = st.seen[k];
                const Trace::Roles roles = RolesAt(st, step.target);
                if (step.asClient)
                {
                    const CastPath path = CastPathOf(k);
                    seen.castSpell = path.start ? path.start : path.go;
                }
                const uint32 own = CastSpellOf(step, seen);
                const std::string base = "step" + U(k + 1);
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = base + suffix;
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    for (size_t i = 0; i < gos.size(); ++i)
                    {
                        std::string go;
                        uint32 spell = 0;
                        if (gos[i]->payload.empty() || !Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) ||
                            sscanf(go.c_str(), "spell=%u", &spell) != 1)
                        {
                            continue;
                        }
                        if (IsCast(step) && spell == own)
                        {
                            seen.goSeen = true;
                            seen.missed = seen.missed || go.find(" hit=[] miss=[target:") != std::string::npos;
                        }
                        else if (spell != kCritAura && spell != CasterAura())
                        {
                            seen.triggered.push_back(spell);
                        }
                    }
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size() && !step.swing; ++i)
                    {
                        if (logs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                        if (fields.size() == 1 &&
                            sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                            spell == own && !seen.logRead)
                        {
                            seen.logRead = true;
                            seen.crit = crit != 0;
                            seen.amount = amount;
                        }
                    }
                    std::vector<Recorder::Seen const*> updates = m_rec.SeenIn(window, SMSG_THREAT_UPDATE);
                    for (size_t i = 0; i < updates.size(); ++i)
                    {
                        std::string text;
                        uint32 value = 0;
                        Recorder::Seen const& update = *updates[i];
                        if (!update.payload.empty() && Trace::DecodeThreatUpdate(&update.payload[0], update.payload.size(), roles, text) &&
                            text.find(" count=1 list=[self:") != std::string::npos &&
                            sscanf(text.c_str() + text.find("list=[self:"), "list=[self:%u]", &value) == 1)
                        {
                            seen.threatUpdate = value;
                        }
                    }
                }
            }
        }

        void ReadStage(St& st, Player* p, Creature* target, uint32 level)
        {
            const uint32 points = TalentMgr::CalculateTalentsPoints(p->getLevel(), p->getClass(), sGameConfig.getConfig(CONFIG_FLOAT_RATE_TALENT), 0);
            uint32 spent = 0;
            std::string known;
            bool allKnown = true;
            uint32 tree = 0;
            for (size_t i = 0; i < st.talents.size(); ++i)
            {
                TalentEntry const* talent = sTalentStore.LookupEntry(st.talents[i].first);
                const uint32 rankSpell = talent ? talent->SpellRank[st.talents[i].second] : 0;
                const bool has = rankSpell && p->HasSpell(rankSpell);
                allKnown = allKnown && has;
                spent += st.talents[i].second + 1;
                tree = tree ? tree : (talent ? talent->TabID : 0);
                known += (known.empty() ? "" : ", ") + U(rankSpell) + (has ? "" : " (not known)");
            }
            // a staged spell that is a talent's rank takes that rank's points when learned
            const std::vector<uint32> learned = Spells();
            for (size_t i = 0; i < learned.size(); ++i)
            {
                if (TalentSpellPos const* pos = GetTalentSpellPos(learned[i]))
                {
                    spent += pos->rank + 1;
                }
            }
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "level %u; free talent points %u of the level's %u, %u spent, primary tree %u; glyph slots open 0x%x; spell crit %.2f%% (fire); mana %u/%u, health %u/%u; target %u level %u, health %u/%u",
                     p->getLevel(), p->GetTalentMgr().FreePoints(), points, spent, p->GetTalentMgr().PrimaryTree(p->GetTalentMgr().ActiveSpec()),
                     p->GetUInt32Value(PLAYER_GLYPHS_ENABLED), p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE),
                     p->GetPower(POWER_MANA), p->GetMaxPower(POWER_MANA), p->GetHealth(), p->GetMaxHealth(), target->GetEntry(),
                     target->getLevel(), target->GetHealth(), target->GetMaxHealth());
            st.stagedText = std::string(buf) + "; talent ranks known: " + (known.empty() ? std::string("none") : known) +
                            "; holders that can proc: self [" + ProcHolders(p) + "] (by chance [" + LiveProcHolders(p) + "]), target [" +
                            ProcHolders(target) + "]";
            st.stagedOk = st.staged && p->getLevel() == level && p->GetTalentMgr().FreePoints() + spent == points && allKnown &&
                          p->GetTalentMgr().PrimaryTree(p->GetTalentMgr().ActiveSpec()) == tree &&
                          p->GetUInt32Value(PLAYER_GLYPHS_ENABLED) == 0x1ff && target->getLevel() == TargetLevel();
            const std::vector<uint32> glyphs = Glyphs();
            if (!glyphs.empty() || HasteRating() || MasteryRating())
            {
                std::string glyphText;
                for (size_t i = 0; i < glyphs.size(); ++i)
                {
                    GlyphPropertiesEntry const* gp = sGlyphPropertiesStore.LookupEntry(glyphs[i]);
                    const bool applied = gp && p->HasAura(gp->SpellID);
                    st.stagedOk = st.stagedOk && applied;
                    glyphText += (glyphText.empty() ? "" : ", ") + U(glyphs[i]) + " (aura " + U(gp ? gp->SpellID : 0) +
                                 (applied ? " on him)" : " not on him)");
                }
                const uint32 haste = p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_HASTE_SPELL);
                const uint32 mastery = p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_MASTERY);
                st.stagedOk = st.stagedOk && haste == uint32(HasteRating()) && mastery == uint32(MasteryRating());
                snprintf(buf, sizeof(buf), "; spell haste rating %u (%.2f%%), mastery rating %u (mastery %.2f)", haste,
                         p->GetRatingBonusValue(CR_HASTE_SPELL), mastery, p->GetFloatValue(PLAYER_MASTERY));
                st.stagedText += "; glyphs " + (glyphText.empty() ? std::string("none") : glyphText) + buf;
            }
            st.stagedCastSpeed = p->GetFloatValue(UNIT_MOD_CAST_SPEED);
            st.stagedCastHaste = p->GetFloatValue(UNIT_MOD_CAST_HASTE);
            snprintf(buf, sizeof(buf), "; cast-speed field %.6f, cast-haste field %.6f", st.stagedCastSpeed, st.stagedCastHaste);
            st.stagedText += buf;
            st.stagedFireCrit = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE);
            m_rec.Note("staged: " + st.stagedText);
        }

        /// The stage read's note of the spell modifier reads the scenario lists (Modifiers): each `(spell, op, base)`
        /// through his modifier manager's SpellModMgr::ApplySpellMod, the values the engine's own calls take, kept in
        /// st.modifiers in the list's order. Nothing is noted for an empty list.
        void ReadModifiers(St& st, Player* p)
        {
            const std::vector<ModifierRead> reads = Modifiers();
            if (reads.empty() || !st.staged)
            {
                return;
            }
            SpellModMgr* mods = p->GetSpellMods();
            std::string text;
            for (size_t i = 0; i < reads.size(); ++i)
            {
                float value = reads[i].base;
                if (mods)
                {
                    mods->ApplySpellMod(reads[i].spell, reads[i].op, value);
                }
                st.modifiers.push_back(value);
                char buf[96];
                snprintf(buf, sizeof(buf), "(%u, op %u, %.0f) -> %.2f", reads[i].spell, uint32(reads[i].op), reads[i].base, value);
                text += (text.empty() ? "" : "; ") + std::string(buf);
            }
            m_rec.Note("spell modifiers: " + text);
        }

        void Finish(St& st)
        {
            m_rec.End();   // the sink comes off before the verdict
            Player* p = sPlayerRegistry.Find(st.player);
            if (!p)
            {
                Verdict(Invalid("the harness player was gone at the verdict"));
                return;
            }
            const std::string persistence = NoPersistence(p, st.achievementsAtSpawn, st.levelSet, st.noReachLevel, st.dealsDamage, &st.plan);
            if (!st.staged)
            {
                const std::string why = "INVALID(StagePlayer refused; its ERR line names the step)";
                Verdict(Compose({ st.templateOk, why, why, why, why, persistence, DigestValue("the first step") }));
                return;
            }
            ReadSeen(st);
            std::string steps, stepsText, stepsBug, triggered;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                Step const& step = st.steps[k];
                Seen const& seen = st.seen[k];
                const uint32 cast = CastSpellOf(step, seen);
                const std::string spellText = U(step.spell) + (cast != step.spell ? ">" + U(cast) : std::string());
                std::string value;
                if (!seen.ran || seen.refused || !seen.read)
                {
                    value = seen.refused ? "refused" : "unread";
                }
                else if (step.swing)
                {
                    value = seen.healthAfter < seen.healthBefore ? "hit" : "swing";
                }
                else if (step.blow)
                {
                    value = "blow";
                }
                else if (step.fullHealth)
                {
                    value = "full";
                }
                else if (step.remove)
                {
                    value = step.mode == AURA_REMOVE_BY_DISPEL ? "dispel" : "remove";
                }
                else if (step.wait)
                {
                    value = "wait";
                }
                else if (step.selfRemoval)
                {
                    value = "cancel";
                }
                else if (step.healthPct)
                {
                    value = "health" + U(step.healthPct);
                }
                else if (step.kill)
                {
                    value = "kill";
                }
                else if (step.aura)
                {
                    value = spellText + (seen.missed ? "miss" : "") + (TargetCount() > 1 ? "@" + U(step.target + 1) : std::string());
                }
                else if (seen.logRead)
                {
                    value = spellText + (seen.crit ? "c" : "");
                }
                else
                {
                    value = spellText + (seen.missed ? "miss" : "none");
                }
                const bool castBug = step.aura ? seen.missed && !step.refusal
                                     : (step.refusal || step.petCast) ? false
                                     : !seen.logRead || (!step.critAllowed && seen.crit != step.crit);
                const bool bug = !seen.ran || seen.refused || !seen.read || (IsCast(step) && castBug);
                if (bug && stepsBug.empty())
                {
                    stepsBug = "step " + U(k + 1) + ": " + value +
                               (!IsCast(step) ? std::string() : step.aura ? ", the aura was wanted"
                                                              : step.crit ? ", a crit was wanted" : ", no crit was wanted");
                }
                steps += (k ? "," : "") + value;
                if (IsCast(step) && seen.logRead)
                {
                    stepsText += (stepsText.empty() ? "" : "; ") + U(k + 1) + " " + U(seen.amount) + ", health " + U(seen.healthBefore) + "->" +
                                 U(seen.healthAfter);
                }
                std::string fired;
                for (size_t i = 0; i < seen.triggered.size(); ++i)
                {
                    fired += (i ? "+" : "") + U(seen.triggered[i]);
                }
                triggered += (k ? "," : "") + (fired.empty() ? std::string("-") : fired);
            }
            const std::string staged = std::string(st.stagedOk ? "OK" : "BUG") + "(" + st.stagedText + ")";
            const std::string stepsValue = stepsBug.empty() ? steps + "(the amounts: " + (stepsText.empty() ? std::string("none") : stepsText) + ")"
                                                            : "BUG(" + steps + ": " + stepsBug + ")";
            Verdict(Compose({ st.templateOk, staged, stepsValue, triggered, Own(st), persistence, DigestValue("the first step") }));
        }

        uint8 m_classId;                    ///< the class of the human the scenario stages
    };
}

#endif
