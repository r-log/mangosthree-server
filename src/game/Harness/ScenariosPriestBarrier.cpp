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

#include "StagedCasterScenario.h"

#include <cstdio>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestBarrier.cpp
 * @brief The priest's Power Word: Barrier scenario, order 1212: his barrier at his feet with a friendly Camel inside
 * it, a Camel's hit on him inside it and outside it, Glyph of Power Word: Barrier's healing inside it, and the
 * barrier's end.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1212 `priest-power-word-barrier`: Power Word: Barrier (62618) summons one barrier, creature 33248, at the
         * point it is cast at, whoever stands there; the barrier channels 81781 at itself, whose two periodic
         * triggers cast, every 100 ms, 81782 (damage taken at its effect 0 value, -25%, and pushback resisted at its
         * effect 1 value, 100%) and, while he holds Glyph of Power Word: Barrier (55689), 90785 (healing taken at its
         * effect 0 value, +10%) at the units friendly to him within the radius of 62618's effect 0 (7 yards); it
         * stands for 62618's duration (10 s).
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with no talents and Power Word:
         * Barrier learned; the Camel ten yards in front of him and a second Camel kFriendYards in front of him.
         * Steps, each a wait whose own read comes right after it acts:
         *  1. the second Camel given his faction by call, so it is friendly to him; his mana set to its maximum by
         *     call; then his 62618 at the point where he stands, as the cast handler starts a ground cast (a spell
         *     whose targets hold the point alone); right after it, the live 33248 within kSearchYards of him counted,
         *     the first kept and any other taken away by call (Creature::ForcedDespawn) before its first update, so
         *     that a cast that summons more barriers than one leaves the run one barrier's casts and packets;
         *  2. the barriers: every live 33248 within kSearchYards of him, the first one's 81781, faction and owner;
         *     81782 on him and on the second Camel, his pushback resisted (SPELL_AURA_REDUCE_PUSHBACK), 90785 on him,
         *     his healing taken modifier; then the Camel's hit on him;
         *  3. he is moved kAwayYards behind the point, as his client's movement word says it
         *     (MovementHandlers::HandleMoverRelocation); the areas under that place and under the point, which his
         *     movement explores (Player::CheckAreaExploreAndOutdoor), are added to the plan's explored areas first;
         *  4. 81782 on him, his pushback resisted and his distance from the barrier; then the Camel's hit on him;
         *  5. Glyph of Power Word: Barrier's aura (55689) put on him as the GM aura command puts it; he is moved back
         *     to the point;
         *  6. 81782 and 90785 on him, his healing taken modifier;
         *  7. the barriers, kLateMs after the cast;
         *  8. the barriers, kEndMs after the cast; 81782 and 90785 on him.
         * The Camel's hit is kHit physical damage through the calls a melee hit makes for what its victim takes
         * (Unit::MeleeDamageBonusTaken, which reads his damage-taken auras), then those the `.damage` command makes
         * (his CalculateDamageAbsorbAndResist, then the Camel's DealDamageMods, DealDamage and SendAttackStateUpdate),
         * his health set to its maximum first; the hit is the health he lost.
         *
         * `barrier`: `barriers=<live 33248 right after the cast>,channel=<81781 on the first at step 2: 81781,
         * none>,faction=<its faction>/<his>,owner=<its owner: him, other>,ward=<81782's effect 0 on him at step 2>/<on
         * the second Camel at step 2>/<on him at step 4>/<at step 6>,pushback=<his pushback resisted at step 2>/<at
         * step 4>,hit=<the hit at step 2>/<at step 4>,glyph=<90785's effect 0 on him at step 2>/<at step
         * 6>,healing=<his healing taken modifier at step 2>/<at step 6>,life=<live 33248 at step 7>/<at step
         * 8>,end=<81782's effect 0 on him at step 8>/<90785's>`, `none` for an aura he does not hold. OK at the second
         * Camel friendly to him at the cast, one barrier right after the cast and at step 2, holding 81781, of his
         * faction and his, ward=<W>/<W>/none/<W>, pushback=<P>/0, hit=<kHit x (100 + W) / 100, one point either
         * way>/<kHit>, glyph=none/<G>, healing=0/<G>, life=1/0 and end=none/none, where W is 81782's effect 0 value in
         * the client's data (-25), P its effect 1 value (100) and G 90785's effect 0 value (10).
         */
        class PriestPowerWordBarrier : public StagedCasterScenario
        {
        public:
            PriestPowerWordBarrier()
                : StagedCasterScenario("priest-power-word-barrier", 1212, "barrier", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return std::vector<std::pair<uint32, uint32> >();
            }
            std::vector<uint32> Spells() const override { return { kBarrier }; }
            bool TakesDamage() const override { return true; }

            /// The second Camel, kFriendYards in front of him: the first stands TargetDistance() yards in front.
            std::vector<std::pair<float, float> > ExtraTargets() const override
            {
                return { std::make_pair(kFriendYards - TargetDistance(), 0.0f) };
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs),
                         Wait(kLateMs - kGlyphReadStep * kStepMs), Wait(kEndMs - kLateMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 62618: 26 fields, fnv 95ee16ea
                    { 62618, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 33248 }, { "eff0.EffectMiscValue_1", 3149 }, { "eff0.EffectRadiusIndex_0", 37 }, { "eff0.ImplicitTarget_0", 31 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 81781: 49 fields, fnv d61a9818
                    { 81781, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 100 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRadiusIndex_0", 37 }, { "eff0.EffectTriggerSpell", 81782 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 100 }, { "eff1.EffectBasePoints", 1 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 90785 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 81782: 49 fields, fnv b01c4305
                    { 81782, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -25 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRadiusIndex_0", 37 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 30 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 149 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 127 }, { "eff1.EffectMiscValue_1", 15 }, { "eff1.EffectRadiusIndex_0", 37 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 30 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 90785: 26 fields, fnv 588ebf42
                    { 90785, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 118 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectRadiusIndex_0", 37 }, { "eff0.EffectRadiusIndex_1", 37 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 30 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 55689: 26 fields, fnv f5664328
                    { 55689, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 1 }, { "eff0.EffectSpellClassMask.Flags", 274877906944 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Creature* friendly = st.targets.size() > 1 ? Get(st.targets[1]) : NULL;
                switch (k)
                {
                    case kCastStep:
                    {
                        if (!friendly)
                        {
                            m_rec.Note("no second Camel");
                            reads["friendly"] = 0;
                            break;
                        }
                        friendly->setFaction(p->getFaction());
                        reads["friendly"] = p->IsFriendlyTo(friendly) ? 1 : 0;
                        m_rec.Note("the second Camel given his faction " + U(p->getFaction()) + " by call: " +
                                   (reads["friendly"] ? "friendly to him" : "not friendly to him") + ", " +
                                   TwoDecimals(p->Where().DistanceTo(friendly->Where())) + " yards from him");
                        CastAtPoint(p, kBarrier, p->Where().X(), p->Where().Y(), p->Where().Z(), uint8(k + 1));
                        reads["summoned"] = KeepOneBarrier(p);
                        break;
                    }
                    case kInsideStep:
                    {
                        ReadBarriers(reads, p);
                        reads["ward"] = Amount(p, kWard);
                        reads["friendWard"] = Amount(friendly, kWard);
                        reads["pushback"] = p->GetTotalAuraModifier(SPELL_AURA_REDUCE_PUSHBACK);
                        reads["glyph"] = Amount(p, kGlyphSpell);
                        reads["healing"] = p->GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HEALING_PCT);
                        m_rec.Note("his " + U(kWard) + ": " + HolderOf(p, kWard) + ", effect 0 " +
                                   Shown(reads["ward"]) + "; the second Camel's " + U(kWard) + ": " +
                                   HolderOf(friendly, kWard) + ", effect 0 " + Shown(reads["friendWard"]) +
                                   "; his pushback resisted " + Shown(reads["pushback"]) + "; his " + U(kGlyphSpell) +
                                   ": " + HolderOf(p, kGlyphSpell) + "; his healing taken modifier " +
                                   Shown(reads["healing"]));
                        reads["hit"] = Hit(p, target);
                        break;
                    }
                    case kOutStep:
                    {
                        PlanExploration(st, P0.x - kAwayYards, P0.y);
                        PlanExploration(st, P0.x, P0.y);
                        Move(p, P0.x - kAwayYards, P0.y);
                        break;
                    }
                    case kOutsideStep:
                    {
                        reads["ward"] = Amount(p, kWard);
                        reads["pushback"] = p->GetTotalAuraModifier(SPELL_AURA_REDUCE_PUSHBACK);
                        const std::vector<Creature*> barriers = Barriers(p);
                        reads["yards"] = barriers.empty()
                                         ? kUnread
                                         : int64(p->Where().DistanceTo(barriers.front()->Where()) + 0.5f);
                        m_rec.Note("his " + U(kWard) + ": " + HolderOf(p, kWard) + ", effect 0 " +
                                   Shown(reads["ward"]) + "; his pushback resisted " + Shown(reads["pushback"]) +
                                   "; " + Shown(reads["yards"]) + " yards from the barrier");
                        reads["hit"] = Hit(p, target);
                        break;
                    }
                    case kGlyphStep:
                    {
                        PutAura(p, kGlyph);
                        reads["glyphed"] = p->HasAura(kGlyph) ? 1 : 0;
                        m_rec.Note("his " + U(kGlyph) + " put on him as the GM aura command puts it: " +
                                   HolderOf(p, kGlyph));
                        Move(p, P0.x, P0.y);
                        break;
                    }
                    case kGlyphReadStep:
                    {
                        reads["ward"] = Amount(p, kWard);
                        reads["glyph"] = Amount(p, kGlyphSpell);
                        reads["healing"] = p->GetMaxPositiveAuraModifier(SPELL_AURA_MOD_HEALING_PCT);
                        m_rec.Note("his " + U(kWard) + ": " + HolderOf(p, kWard) + ", effect 0 " +
                                   Shown(reads["ward"]) + "; his " + U(kGlyphSpell) + ": " +
                                   HolderOf(p, kGlyphSpell) + ", effect 0 " + Shown(reads["glyph"]) +
                                   "; his healing taken modifier " + Shown(reads["healing"]));
                        break;
                    }
                    case kLateStep:
                    {
                        reads["barriers"] = int64(Barriers(p).size());
                        m_rec.Note(U(kLateMs) + " ms after the cast: " + Shown(reads["barriers"]) + " live " +
                                   U(kBarrierCreature) + " within " + U(uint32(kSearchYards)) + " yards of him");
                        break;
                    }
                    case kEndStep:
                    {
                        reads["barriers"] = int64(Barriers(p).size());
                        reads["ward"] = Amount(p, kWard);
                        reads["glyph"] = Amount(p, kGlyphSpell);
                        m_rec.Note(U(kEndMs) + " ms after the cast: " + Shown(reads["barriers"]) + " live " +
                                   U(kBarrierCreature) + " within " + U(uint32(kSearchYards)) + " yards of him; his " +
                                   U(kWard) + ": " + HolderOf(p, kWard) + "; his " + U(kGlyphSpell) + ": " +
                                   HolderOf(p, kGlyphSpell));
                        break;
                    }
                    default:
                        break;
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 friendly = Kept(st, kCastStep, "friendly");
                const int64 barriers = Kept(st, kCastStep, "summoned");
                const int64 kept = Kept(st, kInsideStep, "barriers");
                const int64 channel = Kept(st, kInsideStep, "channel");
                const int64 faction = Kept(st, kInsideStep, "faction");
                const int64 his = Kept(st, kInsideStep, "his");
                const int64 owned = Kept(st, kInsideStep, "owned");
                const int64 wardIn = Kept(st, kInsideStep, "ward");
                const int64 wardFriend = Kept(st, kInsideStep, "friendWard");
                const int64 wardOut = Kept(st, kOutsideStep, "ward");
                const int64 wardBack = Kept(st, kGlyphReadStep, "ward");
                const int64 pushbackIn = Kept(st, kInsideStep, "pushback");
                const int64 pushbackOut = Kept(st, kOutsideStep, "pushback");
                const int64 hitIn = Kept(st, kInsideStep, "hit");
                const int64 hitOut = Kept(st, kOutsideStep, "hit");
                const int64 glyphBefore = Kept(st, kInsideStep, "glyph");
                const int64 glyphAfter = Kept(st, kGlyphReadStep, "glyph");
                const int64 healingBefore = Kept(st, kInsideStep, "healing");
                const int64 healingAfter = Kept(st, kGlyphReadStep, "healing");
                const int64 lifeLate = Kept(st, kLateStep, "barriers");
                const int64 lifeEnd = Kept(st, kEndStep, "barriers");
                const int64 wardEnd = Kept(st, kEndStep, "ward");
                const int64 glyphEnd = Kept(st, kEndStep, "glyph");

                const int64 ward = DataValue(kWard, EFFECT_INDEX_0);
                const int64 pushback = DataValue(kWard, EFFECT_INDEX_1);
                const int64 glyph = DataValue(kGlyphSpell, EFFECT_INDEX_0);
                const int64 reduced = int64(kHit) * (100 + ward) / 100;
                const bool hitOk = hitIn >= 0 && reduced >= 0 && hitOut == int64(kHit) &&
                                   InBand(uint32(hitIn), uint32(reduced), uint32(reduced));
                const bool ok = friendly == 1 && barriers == 1 && kept == 1 && channel == 1 && owned == 1 &&
                                faction == his && ward < 0 && wardIn == ward && wardFriend == ward && wardOut == kNone &&
                                wardBack == ward && pushback > 0 && pushbackIn == pushback && pushbackOut == 0 &&
                                hitOk && glyph > 0 && glyphBefore == kNone && glyphAfter == glyph &&
                                healingBefore == 0 && healingAfter == glyph && lifeLate == 1 && lifeEnd == 0 &&
                                wardEnd == kNone && glyphEnd == kNone;

                const std::string value =
                    "barriers=" + Shown(barriers) + ",channel=" + (channel == 1 ? U(kChannel) : std::string("none")) +
                    ",faction=" + Shown(faction) + "/" + Shown(his) +
                    ",owner=" + (owned == 1 ? std::string("him") : std::string("other")) +
                    ",ward=" + Shown(wardIn) + "/" + Shown(wardFriend) + "/" + Shown(wardOut) + "/" + Shown(wardBack) +
                    ",pushback=" + Shown(pushbackIn) + "/" + Shown(pushbackOut) +
                    ",hit=" + Shown(hitIn) + "/" + Shown(hitOut) + ",glyph=" + Shown(glyphBefore) + "/" +
                    Shown(glyphAfter) + ",healing=" + Shown(healingBefore) + "/" + Shown(healingAfter) +
                    ",life=" + Shown(lifeLate) + "/" + Shown(lifeEnd) + ",end=" + Shown(wardEnd) + "/" +
                    Shown(glyphEnd);
                const std::string detail =
                    "wanted the second Camel friendly to him at the cast and one barrier (" + U(kBarrierCreature) +
                    ") from his " + U(kBarrier) + " at his feet, counted right after the cast (any more are taken " +
                    "away by call, so that the run keeps one), holding its " + U(kChannel) + ", of his faction and " +
                    "owned by him; " + U(kWard) + " at " + std::to_string(ward) + " (its effect 0 value) on him and " +
                    "on the second Camel inside it, none on him " + U(uint32(kAwayYards)) + " yards away, again on " +
                    "him back inside; his pushback resisted " + std::to_string(pushback) + " (its effect 1 value) " +
                    "inside and 0 outside; the Camel's hit of " + U(kHit) + " taking " + std::to_string(reduced) +
                    " inside, one point either way, and " + U(kHit) + " outside; no " + U(kGlyphSpell) + " before " +
                    "Glyph of Power Word: Barrier (" + U(kGlyph) + ") and " + U(kGlyphSpell) + " at " +
                    std::to_string(glyph) + " (its effect 0 value) with it, his healing taken modifier 0, then " +
                    std::to_string(glyph) + "; the barrier standing " + U(kLateMs) + " ms after the cast and gone, " +
                    "with " + U(kWard) + " and " + U(kGlyphSpell) + " off him, " + U(kEndMs) + " ms after it: it " +
                    "stands for " + U(kBarrier) + "'s duration, " + std::to_string(BarrierLifeMs()) + " ms, and its " +
                    "casts reach " + TwoDecimals(BarrierYards()) + " yards (" + U(kBarrier) + "'s effect 0 radius)";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kBarrier = 62618;
            static constexpr uint32 kBarrierCreature = 33248;
            static constexpr uint32 kChannel = 81781;
            static constexpr uint32 kWard = 81782;
            static constexpr uint32 kGlyphSpell = 90785;
            static constexpr uint32 kGlyph = 55689;
            static constexpr uint32 kHit = 4000;
            static constexpr float kFriendYards = 3.0f;
            static constexpr float kAwayYards = 15.0f;
            static constexpr float kSearchYards = 30.0f;
            static constexpr size_t kNotedBarriers = 3;
            static constexpr uint32 kStepMs = 1000;
            static constexpr uint32 kLateMs = 9200;
            static constexpr uint32 kEndMs = 11700;
            static constexpr uint32 kCastStep = 0;
            static constexpr uint32 kInsideStep = 1;
            static constexpr uint32 kOutStep = 2;
            static constexpr uint32 kOutsideStep = 3;
            static constexpr uint32 kGlyphStep = 4;
            static constexpr uint32 kGlyphReadStep = 5;
            static constexpr uint32 kLateStep = 6;
            static constexpr uint32 kEndStep = 7;
            static constexpr size_t kSteps = 8;
            static constexpr int64 kNone = -1;
            static constexpr int64 kUnread = -2;

            /// His cast of `spellId` at the point (x, y, z) as the cast handler starts a ground cast: the spellbook
            /// check, his mana set to its maximum, then a spell whose targets hold the point alone, started
            /// (Spell::SpellStart).
            void CastAtPoint(Player* p, uint32 spellId, float x, float y, float z, uint8 castCount)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spellId);
                if (!info || !p->HasActiveSpell(spellId))
                {
                    m_rec.Note("his spellbook refuses " + U(spellId));
                    return;
                }
                p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                SpellCastTargets targets;
                targets.setDestination(x, y, z);
                Spell* spell = new Spell(p, info, false, p->GetObjectGuid(), NULL);
                spell->m_cast_count = castCount;
                spell->m_glyphIndex = 0;
                spell->SpellStart(&targets, NULL);
                char buf[160];
                snprintf(buf, sizeof(buf), "he casts %u at (%.2f, %.2f, %.2f), %.2f yards from him", spellId, x, y, z,
                         p->Where().DistanceTo(Geometry::Vector3(x, y, z)));
                m_rec.Note(buf);
            }

            /// The live 33248 within kSearchYards of him right after his cast, counted; the first stays and every other
            /// one is taken away by call (Creature::ForcedDespawn) before its first update, so that a cast that
            /// summons more than one barrier leaves the run one barrier's casts and packets. Answers the count.
            int64 KeepOneBarrier(Player* p)
            {
                const std::vector<Creature*> barriers = Barriers(p);
                for (size_t i = 1; i < barriers.size(); ++i)
                {
                    barriers[i]->ForcedDespawn();
                }
                m_rec.Note(U(uint32(barriers.size())) + " live " + U(kBarrierCreature) + " within " +
                           U(uint32(kSearchYards)) + " yards of him right after the cast" +
                           (barriers.size() > 1 ? "; all but the first taken away by call" : std::string()));
                return int64(barriers.size());
            }

            /// The area under (x, y), read off the map's terrain, added to the areas his own movement explores
            /// (QuestPlan::exploredAreas): the movement word that moves him there explores it
            /// (Player::CheckAreaExploreAndOutdoor), and noPersistence judges each EXPLORE_AREA criteria the run fires
            /// against those areas.
            void PlanExploration(St& st, float x, float y)
            {
                Load(x, y);
                Map* map = GetMap();
                const uint32 area = map ? map->GetTerrain()->GetAreaId(x, y, Ground(x, y, P0.z)) : 0;
                if (area)
                {
                    st.plan.exploredAreas.insert(area);
                }
                char buf[128];
                snprintf(buf, sizeof(buf), "the area under (%.2f, %.2f): %u, explored by his movement", x, y, area);
                m_rec.Note(buf);
            }

            /// Him moved to (x, y) on the ground, his facing kept, as his client's movement word says it: a word with
            /// no movement flags down the handler the client's own packets go down
            /// (MovementHandlers::HandleMoverRelocation).
            void Move(Player* p, float x, float y)
            {
                MovementInfo word = p->ClientStatus();
                word.SetMovementFlags(MOVEFLAG_NONE);
                word.ChangePosition(x, y, Ground(x, y, P0.z), p->Where().Facing());
                MovementHandlers::HandleMoverRelocation(*SessionOf(*p), p, word);
                char buf[128];
                snprintf(buf, sizeof(buf), "one movement word, no flags: he stands at (%.2f, %.2f, %.2f)",
                         p->Where().X(), p->Where().Y(), p->Where().Z());
                m_rec.Note(buf);
            }

            /// The Camel's hit on him (the class comment's): answers the health he lost; kUnread without the Camel.
            int64 Hit(Player* p, Creature* camel)
            {
                if (!camel)
                {
                    m_rec.Note("no Camel");
                    return kUnread;
                }
                p->SetHealth(p->GetMaxHealth());
                const uint32 before = p->GetHealth();
                const float taken = p->GetTotalAuraMultiplierByMiscMask(SPELL_AURA_MOD_DAMAGE_PERCENT_TAKEN,
                                                                        SPELL_SCHOOL_MASK_NORMAL);
                const uint32 damage = p->MeleeDamageBonusTaken(camel, kHit, BASE_ATTACK);
                uint32 absorb = 0;
                uint32 resist = 0;
                p->CalculateDamageAbsorbAndResist(camel, SPELL_SCHOOL_MASK_NORMAL, DIRECT_DAMAGE, damage, &absorb,
                                                  &resist);
                if (damage > absorb + resist)
                {
                    uint32 left = damage - absorb - resist;
                    camel->DealDamageMods(p, left, &absorb);
                    camel->DealDamage(p, left, NULL, DIRECT_DAMAGE, SPELL_SCHOOL_MASK_NORMAL, NULL, false);
                    camel->SendAttackStateUpdate(HITINFO_NORMALSWING2, p, SPELL_SCHOOL_MASK_NORMAL, left, absorb,
                                                 resist, VICTIMSTATE_NORMAL, 0);
                }
                const uint32 after = p->GetHealth();
                char buf[224];
                snprintf(buf, sizeof(buf),
                         "the Camel hits him for %u physical: his physical damage taken x%.2f, he takes %u, absorbed "
                         "%u, resisted %u; his health %u -> %u", kHit, taken, damage, absorb, resist, before, after);
                m_rec.Note(buf);
                return int64(before) - int64(after);
            }

            /// Every live creature 33248 within kSearchYards of him.
            std::vector<Creature*> Barriers(Player* p) const
            {
                std::vector<Creature*> out;
                if (!p || !p->IsInWorld())
                {
                    return out;
                }
                std::list<Creature*> found;
                MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kBarrierCreature, kSearchYards);
                MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
                Cell::VisitGridObjects(p, searcher, kSearchYards);
                for (Creature* barrier : found)
                {
                    if (barrier->IsAlive())
                    {
                        out.push_back(barrier);
                    }
                }
                return out;
            }

            /// Keeps the barriers: their count, and of the first its 81781 (1 or 0), its faction, his and whether he
            /// owns it; notes the first kNotedBarriers ones' guid, 81781, channel field, unit flags, display, level,
            /// faction, owner, creator and distance from him.
            void ReadBarriers(std::map<std::string, int64>& reads, Player* p)
            {
                const std::vector<Creature*> barriers = Barriers(p);
                Creature* first = barriers.empty() ? NULL : barriers.front();
                reads["barriers"] = int64(barriers.size());
                reads["channel"] = first && first->HasAura(kChannel) ? 1 : 0;
                reads["faction"] = first ? int64(first->getFaction()) : kUnread;
                reads["his"] = int64(p->getFaction());
                reads["owned"] = first && first->GetOwnerGuid() == p->GetObjectGuid() ? 1 : 0;
                std::string text;
                for (size_t i = 0; i < barriers.size() && i < kNotedBarriers; ++i)
                {
                    Creature* barrier = barriers[i];
                    char buf[256];
                    snprintf(buf, sizeof(buf),
                             "%s%u: %u %s, channel %u, unit flags 0x%x, display %u, level %u, faction %u, owner %s, "
                             "creator %s, %.2f yards from him", text.empty() ? "" : "; ", barrier->GetGUIDLow(),
                             kChannel, HolderOf(barrier, kChannel).c_str(), barrier->GetUInt32Value(UNIT_CHANNEL_SPELL),
                             barrier->GetUInt32Value(UNIT_FIELD_FLAGS), barrier->GetDisplayId(), barrier->getLevel(),
                             barrier->getFaction(), barrier->GetOwnerGuid() == p->GetObjectGuid() ? "him" : "other",
                             barrier->GetCreatorGuid() == p->GetObjectGuid() ? "him" : "other",
                             p->Where().DistanceTo(barrier->Where()));
                    text += buf;
                }
                m_rec.Note(U(uint32(barriers.size())) + " live " + U(kBarrierCreature) + " within " +
                           U(uint32(kSearchYards)) + " yards of him" + (text.empty() ? std::string() : ": " + text));
            }

            /// The amount of effect 0 of `unit`'s holder of `spell`; kNone when it holds none.
            static int64 Amount(Unit* unit, uint32 spell)
            {
                Aura* aura = unit ? unit->GetAura(spell, EFFECT_INDEX_0) : NULL;
                return aura ? int64(aura->GetModifier()->m_amount) : kNone;
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// 62618's duration in the client's data, the barrier's life; 0 when the store lacks the spell.
            static int32 BarrierLifeMs()
            {
                SpellEntry const* info = sSpellStore.LookupEntry(kBarrier);
                return info ? GetSpellDuration(info) : 0;
            }

            /// The radius of 62618's effect 0 in the client's data (radius index 37, 7 yards); 0 when the store lacks
            /// the spell or the effect.
            static float BarrierYards()
            {
                SpellEntry const* info = sSpellStore.LookupEntry(kBarrier);
                SpellEffectEntry const* effect = info ? info->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                return effect ? GetSpellRadius(sSpellRadiusStore.LookupEntry(effect->GetRadiusIndex())) : 0.0f;
            }

            /// What ReadOwn kept of step k under `name`; kUnread when the step was not read.
            static int64 Kept(St const& st, uint32 k, char const* name)
            {
                if (k >= st.seen.size())
                {
                    return kUnread;
                }
                std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
                return i == st.seen[k].reads.end() ? kUnread : i->second;
            }

            /// `value` as the record shows it: `none` for kNone, `unread` for kUnread.
            static std::string Shown(int64 value)
            {
                return value == kNone ? std::string("none")
                                      : value == kUnread ? std::string("unread") : std::to_string(value);
            }

            /// `value` with two decimals.
            static std::string TwoDecimals(float value)
            {
                char text[32];
                snprintf(text, sizeof(text), "%.2f", value);
                return text;
            }
        };
    }

    void RegisterPriestBarrierScenarios(Runner& r)
    {
        r.Register(new PriestPowerWordBarrier());
    }
}
