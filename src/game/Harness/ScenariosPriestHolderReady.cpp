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
#include "spells/SpellScriptQueries.h"
#include "Group.h"

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestHolderReady.cpp
 * @brief The engine's aura holder ready state, order 1233: a holder starts unready, and every way a holder reaches a
 * unit but a spell's hit marks it ready as it lands, so the proc loop (Unit::ProcDamageAndSpellFor) passes over the
 * new holder of a spell during that spell's own hit and over no other.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1233 `priest-holder-ready`: the state of an aura holder when it reaches a unit. SpellAuraHolder's state is
         * CREATED (0) when the holder is made, and the proc loop passes over a holder that is not READY (1). A spell's
         * hit marks the holder it adds after the hit's own procs (Spell::DoAllEffectOnTarget), so the new holder of a
         * spell never procs from the hit that puts it on; every other way a holder reaches a unit (the GM aura command,
         * an area aura on a member, a copied aura, a stolen one, a loaded one) marks it as it lands. Before the change
         * the state was never set when the holder was made, so it held what the memory held: READY in memory a freed
         * holder left (a spell's holder then procs from its own hit), CREATED in fresh memory (a holder of another
         * adder then never procs), anything else in other memory.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST), no talent and no spell learned; the
         * Camel ten yards in front of him and a second Camel kSecondCamelSide yards to its side, out of Frost Nova's
         * 12 yards; one more harness priest, level 85 with that level's stats, kFriendSide yards to his side, in his
         * group (each linked to an in-memory Group by Player::SetGroup, with no group row and no member row). Steps,
         * kStepMs apart, each a wait whose work comes right after it acts:
         *  1. ten times, the Camel's Frost Nova root taken off it and his cooldown of Frost Nova (122) taken off him by
         *     call (the cooldown store runs on time(NULL), which the stepped world does not move), then Frost Nova
         *     cast by him at the Camel through the engine's calls (Unit::CastSpell, triggered), with Precision
         *     (13843) put on him first so that no cast misses; the real holder of his 122 on the Camel is looked up
         *     after each hit and counted when it stands. Frost Nova's flags 0xA22A8 include PROC_FLAG_ON_TAKE_PERIODIC,
         *     so the own-spell rule of Unit::IsTriggeredAtSpellProcEvent does not cover its root, which rolls the
         *     hit's own damage against 25 x the Camel's level - 150 (Unit::HandleRemoveByDamageProc): only the holder's
         *     state keeps it out of its own hit's procs;
         *  2. Focused Will's second rank (45243) put on him as the GM aura command puts it (PutAura), his health set
         *     to its maximum and his Focused Will (45242) taken off him, then the Camel's normal melee hit of 1 more
         *     than 45234's percent (10) of his maximum health reaches his procs (Unit::ProcDamageAndSpell with the
         *     melee hit's flags, as 1214 does): the rank procs only when its holder is ready;
         *  3. Shadowform (15473) cast by him at himself, triggered: its shapeshift aura casts Mind Quickening (49868),
         *     whose raid area aura reaches the friend through AreaAura::Update, which adds a holder of it there;
         *  4. the friend's holder of 49868 read; his Shadow Word: Pain (589) put on the Camel as the aura command puts
         *     it and copied from it to the second Camel (CopyAuraHolder, as Impact's spread copies a damage over
         *     time); the copy's holder read.
         * The client data holds the ids: Frost Nova 122 (flags 0xA22A8, a damage effect and a root), Focused Will 45243
         * (flags 0x222A8, a proc trigger of 45242), Mind Quickening 49868 (an area aura on the raid), Shadow Word: Pain
         * 589.
         *
         * `holderReady`: `ownHit=<A>/10,command=<c>,area=<a>,copy=<y>`, where <A> is how many of the ten Frost Novas left
         * its root standing on the Camel after the hit, <c> is 1 when he holds Focused Will (45242) after the Camel's
         * hit, else 0, <a> is 1 when the friend's holder of 49868 is READY, else 0 (0 too when he holds none), and <y>
         * is 1 when the copy's holder on the second Camel is READY, else 0 (0 too when there is none). OK when the line
         * is `ownHit=10/10,command=1,area=1,copy=1`. The detail names the states read, as numbers (0 CREATED, 1 READY,
         * 2 REMOVING, -1 no holder). On a tree where a holder is not marked when it lands the value depends on the
         * heap (BUG whenever a part reads unready or a root broke), so it is recorded, not compared.
         */
        class PriestHolderReady : public StagedCasterScenario
        {
        public:
            PriestHolderReady() : StagedCasterScenario("priest-holder-ready", 1233, "holderReady", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return std::vector<std::pair<uint32, uint32> >();
            }
            bool TakesDamage() const override { return true; }
            std::vector<std::pair<float, float> > ExtraTargets() const override
            {
                return { { 0.0f, kSecondCamelSide } };
            }

            void AfterStage(Player& p) override
            {
                m_friend = ObjectGuid();
                m_group.reset(new Group());
                p.SetGroup(m_group.get(), 0);
                const float y = P0.y + kFriendSide;
                if (Player* friendPlayer = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_friend = friendPlayer->GetObjectGuid();
                    friendPlayer->SetLevel(kFriendLevel);
                    friendPlayer->InitStatsForLevel(true);
                    friendPlayer->SetGroup(m_group.get(), 0);
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 122: 49 fields, fnv 2ec0d9e1
                    { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 45243: 26 fields, fnv 49509179
                    { 45243, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 77449404560 }, { "eff0.EffectTriggerSpell", 45242 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 45242: 26 fields, fnv a3145923
                    { 45242, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectSpellClassMask.Flags", 4966134988432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                if (atRead)
                {
                    if (k == kCopyStep)
                    {
                        LeaveTheGroup(p);
                    }
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                if (k == kNovaStep)
                {
                    CastNovas(reads, p, target);
                }
                else if (k == kCommandStep)
                {
                    HitHim(reads, p, target);
                }
                else if (k == kShadowformStep)
                {
                    BecomeShadow(reads, p);
                }
                else
                {
                    Creature* second = st.targets.size() > 1 ? Get(st.targets[1]) : NULL;
                    ReadArea(reads, p);
                    CopyPain(reads, p, target, second);
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 standing = Kept(st, kNovaStep, "standing");
                const int64 command = Kept(st, kCommandStep, "command");
                const int64 areaState = Kept(st, kCopyStep, "areaState");
                const int64 copyState = Kept(st, kCopyStep, "copyState");
                const bool area = areaState == int64(SPELLAURAHOLDER_STATE_READY);
                const bool copy = copyState == int64(SPELLAURAHOLDER_STATE_READY);
                const bool ok = standing == int64(kNovas) && command == 1 && area && copy;

                const std::string line = "ownHit=" + std::to_string(standing) + "/" + U(kNovas) +
                                         ",command=" + std::to_string(command) + ",area=" + (area ? "1" : "0") +
                                         ",copy=" + (copy ? "1" : "0");
                const std::string detail =
                    "his " + U(kFrostNova) + " on the Camel stood after " + std::to_string(standing) + " of " +
                    U(kNovas) + " hits, its holder's state after the last " +
                    std::to_string(Kept(st, kNovaStep, "novaState")) + "; his " + U(kFocusedWillRank2) +
                    " put on him by the aura command, its state right before the Camel's hit of " +
                    std::to_string(Kept(st, kCommandStep, "damage")) + " " +
                    std::to_string(Kept(st, kCommandStep, "rankState")) + ", his " + U(kFocusedWillBuff) +
                    " right after it " + std::to_string(command) + "; his " + U(kShadowform) + " " +
                    std::to_string(Kept(st, kShadowformStep, "form")) + " and " + U(kMindQuickening) + " " +
                    std::to_string(Kept(st, kShadowformStep, "mind")) + "; the friend's " + U(kMindQuickening) +
                    " state " + std::to_string(areaState) + "; his " + U(kShadowWordPain) + " on the Camel " +
                    std::to_string(Kept(st, kCopyStep, "painHeld")) + ", copied " +
                    std::to_string(Kept(st, kCopyStep, "copied")) + ", the copy's state " +
                    std::to_string(copyState) + " (0 CREATED, 1 READY, 2 REMOVING, -1 no holder); wanted " +
                    "ownHit=" + U(kNovas) + "/" + U(kNovas) + ",command=1,area=1,copy=1";
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kFrostNova = 122;
            static constexpr uint32 kPrecision = 13843;
            static constexpr uint32 kFocusedWillRank1 = 45234;
            static constexpr uint32 kFocusedWillRank2 = 45243;
            static constexpr uint32 kFocusedWillBuff = 45242;
            static constexpr uint32 kShadowform = 15473;
            static constexpr uint32 kMindQuickening = 49868;
            static constexpr uint32 kShadowWordPain = 589;
            static constexpr uint32 kNovas = 10;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 4.0f;
            static constexpr float kSecondCamelSide = 20.0f;
            static constexpr uint32 kStepMs = 2000;
            static constexpr size_t kSteps = 4;
            static constexpr uint32 kNovaStep = 0;
            static constexpr uint32 kCommandStep = 1;
            static constexpr uint32 kShadowformStep = 2;
            static constexpr uint32 kCopyStep = 3;

            ObjectGuid m_friend;                ///< the friend, spawned after the stage
            std::unique_ptr<Group> m_group;     ///< the group he and the friend are linked to; never stored

            /// What ReadOwn kept of step k under `name`; -2 when the step was not read.
            static int64 Kept(St const& st, uint32 k, char const* name)
            {
                if (k >= st.seen.size())
                {
                    return -2;
                }
                std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
                return i == st.seen[k].reads.end() ? -2 : i->second;
            }

            /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
            static int64 DataValue(uint32 spell, SpellEffectIndex index)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spell);
                return info ? int64(info->CalculateSimpleValue(index)) : 0;
            }

            /// The state of `holder`; -1 when there is none.
            static int64 StateOf(SpellAuraHolder const* holder)
            {
                return holder ? int64(holder->GetState()) : -1;
            }

            /// Ten times: his Frost Nova's root taken off the Camel and his cooldown of it off him by call, then
            /// Frost Nova cast by him at the Camel, triggered; the holder of it that stands on the Camel after the hit
            /// is counted (`standing`), its state kept after the last (`novaState`); noted.
            void CastNovas(std::map<std::string, int64>& reads, Player* p, Creature* target)
            {
                if (!p->HasAura(kPrecision))
                {
                    PutAura(p, kPrecision);
                }
                int64 standing = 0;
                int64 state = -1;
                std::string text;
                for (uint32 cast = 0; cast < kNovas; ++cast)
                {
                    target->RemoveAurasDueToSpell(kFrostNova);
                    p->RemoveSpellCooldown(kFrostNova, true);
                    p->CastSpell(target, kFrostNova, true);
                    SpellAuraHolder* root = target->GetSpellAuraHolder(kFrostNova, p->GetObjectGuid());
                    const bool stands = root && !root->IsDeleted();
                    standing += stands ? 1 : 0;
                    state = stands ? StateOf(root) : -1;
                    text += (text.empty() ? "" : ",") + std::string(stands ? "1" : "0");
                }
                reads["standing"] = standing;
                reads["novaState"] = state;
                m_rec.Note("his " + U(kFrostNova) + " at the Camel, its root and his cooldown of it taken off by call "
                           "before each cast, triggered: the root stood after each hit " + text);
            }

            /// His Focused Will, second rank, put on him as the aura command puts it; his health set to its maximum
            /// and his Focused Will (45242) taken off him; then the Camel's normal melee hit of 1 more than the first
            /// rank's percent of his maximum health reaches his procs. Keeps the rank's state right before the hit
            /// (`rankState`), the hit's damage and whether he holds 45242 after it (`command`); noted.
            void HitHim(std::map<std::string, int64>& reads, Player* p, Creature* target)
            {
                PutAura(p, kFocusedWillRank2);
                p->SetHealth(p->GetMaxHealth());
                p->RemoveAurasDueToSpell(kFocusedWillBuff);
                reads["rankState"] = StateOf(p->GetSpellAuraHolder(kFocusedWillRank2, p->GetObjectGuid()));
                const int64 data = DataValue(kFocusedWillRank1, EFFECT_INDEX_1);
                const int64 percent = data > 0 ? data : 0;
                const uint32 damage = uint32(uint64(p->GetMaxHealth()) * uint64(percent) / 100 + 1);
                reads["damage"] = damage;
                target->ProcDamageAndSpell(p, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                           PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                           PROC_EX_NORMAL_HIT, damage, BASE_ATTACK, NULL, 0);
                reads["command"] = p->HasAura(kFocusedWillBuff) ? 1 : 0;
                m_rec.Note("his " + U(kFocusedWillRank2) + " put on him as the aura command puts it; the Camel's " +
                           "normal melee hit of " + U(damage) + " at his " + U(p->GetMaxHealth()) +
                           " health reaches his procs; his " + U(kFocusedWillBuff) + " right after: " +
                           (reads["command"] ? "held" : "not held"));
            }

            /// Shadowform cast by him at himself, triggered; whether he holds it and Mind Quickening, which its
            /// shapeshift aura casts, kept (`form`, `mind`); noted.
            void BecomeShadow(std::map<std::string, int64>& reads, Player* p)
            {
                p->CastSpell(p, kShadowform, true);
                reads["form"] = p->HasAura(kShadowform) ? 1 : 0;
                reads["mind"] = p->HasAura(kMindQuickening) ? 1 : 0;
                m_rec.Note("he casts " + U(kShadowform) + " at himself, triggered: his " + U(kShadowform) +
                           (reads["form"] ? " held" : " not held") + ", his " + U(kMindQuickening) +
                           (reads["mind"] ? " held" : " not held"));
            }

            /// The state of the friend's holder of Mind Quickening, that the area aura's update added; kept
            /// (`areaState`, -1 for none); noted.
            void ReadArea(std::map<std::string, int64>& reads, Player* p)
            {
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                SpellAuraHolder* area = friendPlayer ? friendPlayer->GetSpellAuraHolder(kMindQuickening, p->GetObjectGuid())
                                                     : NULL;
                reads["areaState"] = StateOf(area);
                m_rec.Note(std::string("the friend ") + (friendPlayer ? "was spawned" : "was not spawned") + "; his " +
                           U(kMindQuickening) + " on the friend: " + (area ? "held" : "not held"));
            }

            /// His Shadow Word: Pain put on the Camel as the aura command puts it and copied from it to the second
            /// Camel; whether he holds it on the Camel (`painHeld`), whether the copy was made (`copied`) and the
            /// state of the copy's holder (`copyState`, -1 for none) kept; noted.
            void CopyPain(std::map<std::string, int64>& reads, Player* p, Creature* target, Creature* second)
            {
                PutAura(target, kShadowWordPain, p);
                reads["painHeld"] = target->GetSpellAuraHolder(kShadowWordPain, p->GetObjectGuid()) ? 1 : 0;
                const bool copied = second && CopyAuraHolder(*target, *second, kShadowWordPain, p->GetObjectGuid());
                reads["copied"] = copied ? 1 : 0;
                reads["copyState"] = StateOf(second ? second->GetSpellAuraHolder(kShadowWordPain, p->GetObjectGuid())
                                                    : NULL);
                m_rec.Note("his " + U(kShadowWordPain) + " on the Camel: " + (reads["painHeld"] ? "held" : "not held") +
                           "; copied to the second Camel: " + (copied ? "yes" : "no"));
            }

            /// He and the friend leave the group, which is deleted.
            void LeaveTheGroup(Player* p)
            {
                if (!m_group)
                {
                    return;
                }
                if (p->GetGroup() == m_group.get())
                {
                    p->SetGroup(NULL);
                }
                Player* friendPlayer = m_friend ? sPlayerRegistry.Find(m_friend) : NULL;
                if (friendPlayer && friendPlayer->GetGroup() == m_group.get())
                {
                    friendPlayer->SetGroup(NULL);
                }
                m_group.reset();
                m_rec.Note("he and the friend left the group");
            }
        };
    }

    void RegisterPriestHolderReadyScenarios(Runner& r)
    {
        r.Register(new PriestHolderReady());
    }
}
