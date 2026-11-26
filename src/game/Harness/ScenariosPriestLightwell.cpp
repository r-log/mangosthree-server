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
 * @file ScenariosPriestLightwell.cpp
 * @brief The Holy priest's Lightwell scenario, order 1230: his Lightwell with Glyph of Lightwell, its charges, the
 * clicks of a friend, the Lightwell Renew each gives with the priest as its caster, the hits that cancel it and those
 * that do not, the ticks of the Renew a lesser hit left, and the Lightwell's end with its last charge.
 */

namespace Harness
{
    namespace
    {
        /**
         * S1230 `priest-lightwell`: Lightwell (724) summons one Lightwell, creature 31897, at the point it is cast at
         * for 3 minutes; on its first update it takes the priest as its owner and creator and holds Lightwell Charges
         * (59907, cast by him), 10 charges and the 5 of Glyph of Lightwell (55673). A click on it (the click row of the
         * world: the Lightwell casts 60123 at the player who clicked) gives that player Lightwell Renew (7001, 6
         * seconds, a tick every 2 seconds) with the priest as its caster, so that each tick heals for the Renew's
         * scaled amount and 0.308 of his spell power (the spell bonus row's) and is his heal to every proc of his;
         * it spends one charge, and the Lightwell goes with the last. A hit its holder takes of at least 724's effect 1
         * percent (30) of his maximum health takes the Renew off him (its one proc charge); a smaller hit leaves it. The
         * Lightwell is no part of its priest's combat, so that a click is never refused for its combat state. The 4.3.4
         * client data holds the values; the retail 4.3.4 sniff (build 15595, 2012-05-28) holds no Lightwell.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with no talents, Lightwell learned,
         * Glyph of Lightwell (glyph 258) and kSpellPower spell power; the Camel ten yards in front of him; and, once he
         * is staged, a second harness player, a human priest taken to level 85 with that level's stats, kFriendSide
         * yards to his side: the friend, who makes every click. The friend is never in combat at a click (the handler
         * refuses a player in combat outside a dungeon, which no step relies on): the Camel's hits on him are the proc
         * call a melee hit makes on its victim (Unit::ProcDamageAndSpell with the hit's flags, its kind and its
         * damage), which puts nobody in combat, and should he be in combat anyway it is taken off him by call
         * (Unit::CombatStop) before the click, which the line counts. His health is set to half its maximum before
         * each click, so that every tick heals. His Holy spell crit is set to -100 percent by call before the second click, so that the
         * ticks of the Renew it gives never crit. Steps, each a wait whose action comes right after it starts, kStepMs
         * apart:
         *  1. the data's values read; his Lightwell at his feet, as the cast handler starts a ground cast (his mana set
         *     to its maximum first);
         *  2. the Lightwells: every live 31897 within kSearchYards of him counted, the first kept and any other taken
         *     away by call (Creature::ForcedDespawn); the first one's owner, creator and 59907; then he enters combat
         *     with the Camel (Unit::SetInCombatState, five seconds of it) and the combat state of him and the
         *     Lightwell is read;
         *  3. the combat states read; the friend's health set to half; his click (CMSG_SPELLCLICK to the spell-click
         *     handler); the charges and the friend's 7001 read;
         *  4. the combat states read; the Camel's hit of 31 percent of the friend's maximum health; his 7001
         *     read;
         *  5. the combat states read; the friend's health set to half and the crit sheet lowered; his second click;
         *     the charges and his 7001 read;
         *  6. the combat states read; the Camel's hit of 29 percent; his 7001 read;
         *  7. to 11. waits: the second Renew's ticks fall 2, 4 and 6 seconds after step 5;
         *  12. the Lightwell's remaining charges set to one by call; the friend's health set to half; his third click;
         *      the charges and his 7001 read;
         *  13. the Lightwell read: gone, or taken away by call when it still stands.
         * Every tick log of his 7001 is read from the packets the recorder kept, by the unit it names; 7001 sends no
         * SMSG_SPELL_GO of its own, so its casts are never counted as GOs.
         *
         * `lightwell`: `wells=<live 31897 right after the cast>,owner=<its owner and creator: him, other>,
         * holder=<the caster of its 59907: him, other, none>,charges=<its charges at step 2>/<after the click of step
         * 3>/<after step 4's hit>/<after the click of step 5>/<after step 6's hit>/<after step 12's click>,
         * renew=<the caster of the friend's 7001 after each of the three clicks: him, other, none>,hits=<the friend's
         * 7001 after step 4's hit: gone, kept>/<after step 6's: gone, kept>,ticks=<the tick logs of 7001 about the
         * friend>x<the amount of the first>,combat=<the reads with him in combat>/<the reads with the Lightwell in
         * it>/<the clicks the friend made in combat>,cleared=<the clicks before which his combat was taken off by
         * call>,gone=<1 when the Lightwell was gone at step 13>`. With C the
         * charges 59907's data holds and G Glyph of Lightwell's effect 0 value, OK when the friend was spawned and the
         * glyph held, wells=1, owner=him, holder=him, charges=C+G/C+G-1/C+G-1/C+G-2/C+G-2/0, renew=him/him/him,
         * hits=gone/kept, ticks=3 x the amount the second Renew holds, which is its scaled amount before the done side
         * and 0.308 of his spell power, one point either way, none of them a crit, combat=5/0/0 (five reads, each with
         * him in combat and the Lightwell out of it, and no click made in combat) and gone=1; cleared is recorded, not
         * judged. Master records no 59907 and a click that gives nothing.
         */
        class PriestLightwell : public StagedCasterScenario
        {
        public:
            PriestLightwell() : StagedCasterScenario("priest-lightwell", 1230, "lightwell", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return std::vector<std::pair<uint32, uint32> >();
            }
            std::vector<uint32> Spells() const override { return { kLightwell }; }
            std::vector<uint32> Glyphs() const override { return { kGlyphOfLightwellId }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            void AfterStage(Player& /*p*/) override
            {
                m_friend = ObjectGuid();
                m_well = ObjectGuid();
                const float y = P0.y + kFriendSide;
                Load(P0.x, y);
                if (Player* ally = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    ally->SetLevel(kFriendLevel);
                    ally->InitStatsForLevel(true);
                    m_friend = ally->GetObjectGuid();
                }
            }

            std::vector<Step> Steps() const override
            {
                std::vector<Step> steps;
                for (size_t k = 0; k < kSteps; ++k)
                {
                    steps.push_back(Wait(kStepMs));
                }
                return steps;
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 724: 49 fields, fnv 4f5a6c05
                    { 724, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 31897 }, { "eff0.EffectMiscValue_1", 1141 }, { "eff0.ImplicitTarget_0", 87 }, { "eff1.present", 1 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 60123: 26 fields, fnv 4565ecbb
                    { 60123, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 59907: 72 fields, fnv 5d8d899f
                    { 59907, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 34 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 65 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 7001: 26 fields, fnv ba98e430
                    { 7001, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 8 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 55673: 26 fields, fnv 205b56e3
                    { 55673, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 4 }, { "eff0.EffectSpellClassMask.Flags2", 8388608 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Player* ally = sPlayerRegistry.Find(m_friend);
                if (!ally || !target)
                {
                    m_rec.Note("the friend or the Camel was not spawned");
                    return;
                }
                switch (k)
                {
                    case kCastStep:
                        ReadTheData(reads, p);
                        p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                        reads["cast"] = CastAtPoint(p, kLightwell, p->Where().X(), p->Where().Y(), p->Where().Z(),
                                                    uint8(k + 1)) ? 1 : 0;
                        break;
                    case kCountStep:
                    {
                        reads["wells"] = KeepOneWell(p);
                        Creature* well = Well(p);
                        ReadWell(reads, p, well);
                        p->SetInCombatState(true, target);
                        ReadCombat(reads, p, well);
                        break;
                    }
                    case kClickOneStep:
                        Click(reads, p, ally);
                        break;
                    case kBigHitStep:
                        ReadCombat(reads, p, Well(p));
                        reads["damage"] = Hit(ally, target, kBigPercent);
                        reads["renew"] = RenewCaster(p, ally);
                        reads["charges"] = WellCharges(Well(p));
                        break;
                    case kClickTwoStep:
                    {
                        const uint16 field = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_HOLY);
                        p->SetFloatValue(field, -100.0f);
                        reads["sheet"] = int64(p->GetFloatValue(field));
                        reads["spellPower"] = p->SpellBaseHealingBonusDone(SPELL_SCHOOL_MASK_HOLY);
                        m_rec.Note("his Holy spell crit sheet set to " + Shown(reads["sheet"]) +
                                   " by call, so that the ticks never crit; his spell power for a Holy heal: " +
                                   Shown(reads["spellPower"]));
                        Click(reads, p, ally);
                        break;
                    }
                    case kSmallHitStep:
                        ReadCombat(reads, p, Well(p));
                        reads["damage"] = Hit(ally, target, kSmallPercent);
                        reads["renew"] = RenewCaster(p, ally);
                        reads["renewCharges"] = RenewCharges(ally);
                        reads["charges"] = WellCharges(Well(p));
                        break;
                    case kLastClickStep:
                    {
                        if (SpellAuraHolder* holder = WellHolder(Well(p)))
                        {
                            holder->SetAuraCharges(1);
                            m_rec.Note("the Lightwell's charges set to one by call");
                        }
                        Click(reads, p, ally);
                        break;
                    }
                    case kEndStep:
                    {
                        Creature* well = Well(p);
                        reads["gone"] = well && well->IsAlive() ? 0 : 1;
                        m_rec.Note(std::string("the Lightwell ") + (reads["gone"] ? "is gone" : "still stands") + "; his " +
                                   U(kCharges) + ": " + HolderOf(well, kCharges));
                        if (well && well->IsAlive())
                        {
                            well->ForcedDespawn();
                            m_rec.Note("the Lightwell taken away by call, so that its 3 minutes do not outlive the run");
                        }
                        break;
                    }
                    default:
                        ReadCombat(reads, p, Well(p));
                        break;
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 percent = Kept(st, kCastStep, "percent");
                const int64 chargesData = Kept(st, kCastStep, "chargesData");
                const int64 glyphData = Kept(st, kCastStep, "glyphData");
                const int64 total = chargesData >= 0 && glyphData >= 0 ? chargesData + glyphData : kUnread;
                const bool glyph = Kept(st, kCastStep, "glyph") == 1;
                const bool cast = Kept(st, kCastStep, "cast") == 1;
                const int64 wells = Kept(st, kCountStep, "wells");
                const int64 owner = Kept(st, kCountStep, "owner");
                const int64 holder = Kept(st, kCountStep, "holder");
                const int64 chargesZero = Kept(st, kCountStep, "charges");
                const int64 chargesOne = Kept(st, kClickOneStep, "charges");
                const int64 chargesBig = Kept(st, kBigHitStep, "charges");
                const int64 chargesTwo = Kept(st, kClickTwoStep, "charges");
                const int64 chargesSmall = Kept(st, kSmallHitStep, "charges");
                const int64 chargesLast = Kept(st, kLastClickStep, "charges");
                const int64 renewOne = Kept(st, kClickOneStep, "renew");
                const int64 renewTwo = Kept(st, kClickTwoStep, "renew");
                const int64 renewLast = Kept(st, kLastClickStep, "renew");
                const int64 afterBig = Kept(st, kBigHitStep, "renew");
                const int64 afterSmall = Kept(st, kSmallHitStep, "renew");
                const int64 chargesOfSmall = Kept(st, kSmallHitStep, "renewCharges");
                const int64 gone = Kept(st, kEndStep, "gone");

                // The combat reads: every step from the count to the second hit reads him and the Lightwell.
                int64 priestReads = 0;
                int64 wellReads = 0;
                int64 allyReads = 0;
                int64 cleared = 0;
                for (uint32 k = kCountStep; k <= kSmallHitStep; ++k)
                {
                    priestReads += Kept(st, k, "priestCombat") == 1 ? 1 : 0;
                    wellReads += Kept(st, k, "wellCombat") == 1 ? 1 : 0;
                }
                for (const uint32 k : { kClickOneStep, kClickTwoStep, kLastClickStep })
                {
                    allyReads += Kept(st, k, "allyCombat") == 1 ? 1 : 0;
                    cleared += Kept(st, k, "cleared") == 1 ? 1 : 0;
                }

                // The ticks of his 7001 about the friend up to the last click's step, and the amount the second Renew holds.
                const uint64 self = st.player.GetRawValue();
                const std::vector<TickLog> ticks = Ticks(self, m_friend.GetRawValue());
                const int64 held = Kept(st, kClickTwoStep, "amount");
                const int64 base = Kept(st, kClickTwoStep, "base");
                const int64 spellPower = Kept(st, kClickTwoStep, "spellPower");
                const int64 bonus = spellPower > 0 ? int64(float(spellPower) * kRenewCoefficient) : 0;
                const int64 wanted = base >= 0 ? base + bonus : kUnread;
                bool ticksOk = ticks.size() == kTicks && held > 0 && wanted > 0 &&
                               InBand(uint32(held), uint32(wanted > 1 ? wanted - 1 : 0), uint32(wanted + 1));
                std::string tickText;
                for (size_t i = 0; i < ticks.size(); ++i)
                {
                    ticksOk = ticksOk && !ticks[i].crit && ticks[i].amount == ticks[0].amount &&
                              held >= 0 && ticks[i].amount == uint32(held);
                    tickText += (i ? "," : "") + U(ticks[i].amount) + (ticks[i].crit ? "c" : "") + "@" + U(ticks[i].step + 1);
                }

                const bool chargesOk = total > 0 && chargesZero == total && chargesOne == total - 1 &&
                                       chargesBig == total - 1 && chargesTwo == total - 2 &&
                                       chargesSmall == total - 2 && chargesLast == 0;
                const bool renewOk = renewOne == 1 && renewTwo == 1 && renewLast == 1;
                const bool hitsOk = percent > kSmallPercent && percent <= kBigPercent && afterBig == kNone &&
                                    afterSmall == 1 && chargesOfSmall == 1;
                const bool combatOk = priestReads == kCombatReads && wellReads == 0 && allyReads == 0;
                const bool ok = !m_friend.IsEmpty() && cast && glyph && wells == 1 && owner == 1 && holder == 1 &&
                                chargesOk && renewOk && hitsOk && ticksOk && combatOk && gone == 1;

                const std::string line =
                    "wells=" + Shown(wells) + ",owner=" + (owner == 1 ? "him" : "other") + ",holder=" +
                    (holder == 1 ? "him" : holder == kNone ? "none" : "other") + ",charges=" + Shown(chargesZero) +
                    "/" + Shown(chargesOne) + "/" + Shown(chargesBig) + "/" + Shown(chargesTwo) + "/" +
                    Shown(chargesSmall) + "/" + Shown(chargesLast) + ",renew=" + CasterText(renewOne) + "/" +
                    CasterText(renewTwo) + "/" + CasterText(renewLast) + ",hits=" + (afterBig == kNone ? "gone" : "kept") +
                    "/" + (afterSmall == kNone ? "gone" : "kept") + ",ticks=" + U(uint32(ticks.size())) + "x" +
                    (ticks.empty() ? std::string("none") : U(ticks[0].amount)) + ",combat=" + Shown(priestReads) + "/" +
                    Shown(wellReads) + "/" + Shown(allyReads) + ",cleared=" + Shown(cleared) + ",gone=" + Shown(gone);
                const std::string detail =
                    std::string("wanted the friend spawned, his Lightwell cast (") + (cast ? "started" : "refused") + ") and Glyph " +
                    "of Lightwell (" + U(kGlyphOfLightwell) + ") held (" + (glyph ? "yes" : "no") + "); one Lightwell (" +
                    U(kLightwellCreature) + ") owned and created by him, its " + U(kCharges) + " cast by him, " +
                    Shown(total) + " charges (" + U(kCharges) + "'s " + Shown(chargesData) + " and the glyph's " +
                    Shown(glyphData) + "), one fewer after each of the three clicks (counted " + Shown(chargesZero) + ", " +
                    Shown(chargesOne) + " and " + Shown(chargesTwo) + ", then " + Shown(chargesLast) + " with the last " +
                    "one set by call to 1 first); his " + U(kRenew) + " on the friend after each click with him as its " +
                    "caster; the Camel's hit of " + Shown(Kept(st, kBigHitStep, "damage")) + " (" + U(kBigPercent) +
                    "% of the friend's health) taking it off, 724's effect 1 being " + Shown(percent) + "%, and the hit " +
                    "of " + Shown(Kept(st, kSmallHitStep, "damage")) + " (" + U(kSmallPercent) + "%) leaving it with " +
                    Shown(chargesOfSmall) + " proc charge; " + U(kTicks) + " tick logs of his " + U(kRenew) + " about the " +
                    "friend, each the " + Shown(held) + " the second Renew holds: its scaled amount " + Shown(base) +
                    " and 0.308 of his spell power " + Shown(spellPower) + " (" + Shown(bonus) +
                    "), " + Shown(wanted) + ", one point either way, none a crit (ticks " +
                    (tickText.empty() ? std::string("none") : tickText) + "); him in combat at " + Shown(priestReads) +
                    " of " + U(uint32(kCombatReads)) + " reads and the Lightwell at " + Shown(wellReads) + ", the " +
                    "friend at " + Shown(allyReads) + " of his three clicks, his combat taken off by call before " +
                    Shown(cleared) + " of them; the Lightwell gone at the last step (" + Shown(gone) + ")";
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kLightwell = 724;
            static constexpr uint32 kLightwellCreature = 31897;
            static constexpr uint32 kCharges = 59907;
            static constexpr uint32 kRenew = 7001;
            static constexpr uint32 kGlyphOfLightwell = 55673;
            static constexpr uint32 kGlyphOfLightwellId = 258;
            static constexpr int32 kSpellPower = 3000;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendSide = 3.0f;
            static constexpr float kSearchYards = 30.0f;
            static constexpr float kRenewCoefficient = 0.308f;
            static constexpr uint32 kBigPercent = 31;
            static constexpr uint32 kSmallPercent = 29;
            static constexpr uint32 kStepMs = 1000;
            static constexpr uint32 kCastStep = 0;
            static constexpr uint32 kCountStep = 1;
            static constexpr uint32 kClickOneStep = 2;
            static constexpr uint32 kBigHitStep = 3;
            static constexpr uint32 kClickTwoStep = 4;
            static constexpr uint32 kSmallHitStep = 5;
            static constexpr uint32 kLastClickStep = 11;
            static constexpr uint32 kEndStep = 12;
            static constexpr size_t kSteps = 13;
            static constexpr size_t kTicks = 3;
            static constexpr int64 kCombatReads = 5;
            static constexpr int64 kNone = -1;
            static constexpr int64 kUnread = -2;

            ObjectGuid m_friend;                ///< the friend, spawned after the stage
            ObjectGuid m_well;                  ///< the Lightwell the count kept

            /// One tick log of his Renew about the friend: the 0-based step whose windows hold it, the whole tick and
            /// whether it was a crit.
            struct TickLog
            {
                uint32 step;
                uint32 amount;
                bool crit;
            };

            /// Every tick log of `self`'s 7001 about `target` up to the last click's step, in order. The third Renew,
            /// given at that step, ticks after the run's last window.
            std::vector<TickLog> Ticks(uint64 self, uint64 target) const
            {
                std::vector<TickLog> out;
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    unsigned int number = 0;
                    Trace::PeriodicAuraLog log;
                    if (packets[i].opcode == SMSG_PERIODICAURALOG && !packets[i].payload.empty() &&
                        sscanf(packets[i].window.c_str(), "step%u", &number) == 1 && number > 0 &&
                        Trace::ReadPeriodicAuraLog(&packets[i].payload[0], packets[i].payload.size(), log) &&
                        log.spell == kRenew && log.caster == self && log.target == target && !log.entries.empty() &&
                        number - 1 <= kLastClickStep)
                    {
                        TickLog tick;
                        tick.step = number - 1;
                        tick.amount = log.entries[0].amount + log.entries[0].absorb;
                        tick.crit = log.entries[0].critical != 0;
                        out.push_back(tick);
                    }
                }
                return out;
            }

            /// Before the first cast: the data's values for the Lightwell, its charges and its glyph, whether he holds
            /// the glyph; kept in `reads` and noted.
            void ReadTheData(std::map<std::string, int64>& reads, Player* p)
            {
                SpellEntry const* lightwell = sSpellStore.LookupEntry(kLightwell);
                SpellEntry const* charges = sSpellStore.LookupEntry(kCharges);
                SpellEntry const* glyph = sSpellStore.LookupEntry(kGlyphOfLightwell);
                reads["duration"] = lightwell ? int64(GetSpellDuration(lightwell)) : kNone;
                reads["percent"] = lightwell ? int64(lightwell->CalculateSimpleValue(EFFECT_INDEX_1)) : kNone;
                reads["chargesData"] = charges ? int64(charges->GetProcCharges()) : kNone;
                reads["glyphData"] = glyph ? int64(glyph->CalculateSimpleValue(EFFECT_INDEX_0)) : kNone;
                reads["glyph"] = p->HasAura(kGlyphOfLightwell) ? 1 : 0;
                m_rec.Note(U(kLightwell) + ": " + Shown(reads["duration"]) + " ms, effect 1 " + Shown(reads["percent"]) +
                           "% of the maximum health of a hit that cancels the Renew; " + U(kCharges) + ": " +
                           Shown(reads["chargesData"]) + " proc charges; Glyph of Lightwell (" + U(kGlyphOfLightwell) +
                           "): " + Shown(reads["glyphData"]) + " more, held: " + (reads["glyph"] ? "yes" : "no"));
            }

            /// His cast of `spellId` at the point (x, y, z) as the cast handler starts a ground cast: the spellbook
            /// check, then a spell whose targets hold the point alone, started (Spell::SpellStart). False when his
            /// spellbook refuses the spell.
            bool CastAtPoint(Player* p, uint32 spellId, float x, float y, float z, uint8 castCount)
            {
                SpellEntry const* info = sSpellStore.LookupEntry(spellId);
                if (!info || !p->HasActiveSpell(spellId))
                {
                    m_rec.Note("his spellbook refuses " + U(spellId));
                    return false;
                }
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
                return true;
            }

            /// Every live creature 31897 within kSearchYards of him.
            std::vector<Creature*> Wells(Player* p) const
            {
                std::vector<Creature*> out;
                if (!p || !p->IsInWorld())
                {
                    return out;
                }
                std::list<Creature*> found;
                MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kLightwellCreature, kSearchYards);
                MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
                Cell::VisitGridObjects(p, searcher, kSearchYards);
                for (Creature* well : found)
                {
                    if (well->IsAlive())
                    {
                        out.push_back(well);
                    }
                }
                return out;
            }

            /// The Lightwell the count kept, while it stands; before the count, the first live 31897 within kSearchYards of him.
            Creature* Well(Player* p)
            {
                if (!m_well.IsEmpty())
                {
                    Creature* kept = Get(m_well);
                    return kept && kept->IsAlive() ? kept : NULL;
                }
                const std::vector<Creature*> wells = Wells(p);
                return wells.empty() ? NULL : wells.front();
            }

            /// The live 31897 within kSearchYards of him, counted after his cast (the Lightwell is summoned when its 500 ms cast
            /// ends, so the count comes a step later); the first stays and every other one is taken away by call
            /// (Creature::ForcedDespawn), so that a cast that summons more than one Lightwell leaves the run one
            /// Lightwell's casts and packets. Answers the count.
            int64 KeepOneWell(Player* p)
            {
                const std::vector<Creature*> wells = Wells(p);
                for (size_t i = 1; i < wells.size(); ++i)
                {
                    wells[i]->ForcedDespawn();
                }
                m_well = wells.empty() ? ObjectGuid() : wells.front()->GetObjectGuid();
                m_rec.Note(U(uint32(wells.size())) + " live " + U(kLightwellCreature) + " within " +
                           U(uint32(kSearchYards)) + " yards of him" +
                           (wells.size() > 1 ? "; all but the first taken away by call" : std::string()));
                return int64(wells.size());
            }

            /// The Lightwell's holder of 59907; NULL for none.
            static SpellAuraHolder* WellHolder(Creature* well)
            {
                return well ? well->GetSpellAuraHolder(kCharges) : NULL;
            }

            /// The charges of the Lightwell's 59907; kNone when it holds none.
            static int64 WellCharges(Creature* well)
            {
                SpellAuraHolder* holder = WellHolder(well);
                return holder ? int64(holder->GetAuraCharges()) : kNone;
            }

            /// The Lightwell as the count read it: whether he owns and created it, whether he cast its 59907 and its
            /// charges; noted with its level, faction, unit and NPC flags and distance from him.
            void ReadWell(std::map<std::string, int64>& reads, Player* p, Creature* well)
            {
                reads["owner"] = well && well->GetOwnerGuid() == p->GetObjectGuid() &&
                                 well->GetCreatorGuid() == p->GetObjectGuid() ? 1 : 0;
                SpellAuraHolder* holder = WellHolder(well);
                reads["holder"] = !holder ? kNone : holder->GetCasterGuid() == p->GetObjectGuid() ? 1 : 0;
                reads["charges"] = WellCharges(well);
                if (!well)
                {
                    m_rec.Note("no Lightwell");
                    return;
                }
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "the Lightwell %u: level %u, faction %u, unit flags 0x%x, NPC flags 0x%x, owner %s, creator %s, "
                         "%.2f yards from him; its %u: %s",
                         well->GetGUIDLow(), well->getLevel(), well->getFaction(), well->GetUInt32Value(UNIT_FIELD_FLAGS),
                         well->GetUInt32Value(UNIT_NPC_FLAGS), well->GetOwnerGuid() == p->GetObjectGuid() ? "him" : "other",
                         well->GetCreatorGuid() == p->GetObjectGuid() ? "him" : "other",
                         p->Where().DistanceTo(well->Where()), kCharges, HolderOf(well, kCharges).c_str());
                m_rec.Note(buf);
            }

            /// Whether he and the Lightwell are in combat, kept as `priestCombat` and `wellCombat`; noted.
            void ReadCombat(std::map<std::string, int64>& reads, Player* p, Creature* well)
            {
                reads["priestCombat"] = p->IsInCombat() ? 1 : 0;
                reads["wellCombat"] = well && well->IsInCombat() ? 1 : 0;
                m_rec.Note(std::string("he is ") + (reads["priestCombat"] ? "in" : "not in") + " combat; " +
                           (well ? std::string("the Lightwell is ") + (reads["wellCombat"] ? "in" : "not in") + " combat"
                                 : std::string("there is no Lightwell")));
            }

            /// The friend's click on the Lightwell as the client sends it (CMSG_SPELLCLICK, the Lightwell's guid) to
            /// the spell-click handler, his health set to half first and his combat taken off by call when he is in it
            /// (kept as `cleared`; the state at the click is `allyCombat`); after the click the Lightwell's charges, the friend's 7001 (its caster as `renew`, its
            /// proc charges as `renewCharges`, the tick it holds before and after the done side as `base` and
            /// `amount`) and the combat states.
            void Click(std::map<std::string, int64>& reads, Player* p, Player* ally)
            {
                Creature* well = Well(p);
                ally->SetHealth(ally->GetMaxHealth() / 2);
                reads["cleared"] = 0;
                if (ally->IsInCombat())
                {
                    ally->CombatStop();
                    reads["cleared"] = 1;
                    m_rec.Note("the friend was in combat: cleared by call (Unit::CombatStop), so that the click is not the "
                               "handler's refusal of a player in combat");
                }
                reads["allyCombat"] = ally->IsInCombat() ? 1 : 0;
                ReadCombat(reads, p, well);
                m_rec.Note("the friend's health " + U(ally->GetHealth()) + " of " + U(ally->GetMaxHealth()) +
                           ", " + (reads["allyCombat"] ? "in" : "not in") + " combat; he clicks the Lightwell");
                if (!well)
                {
                    m_rec.Note("no Lightwell to click");
                    reads["charges"] = kNone;
                    reads["renew"] = RenewCaster(p, ally);
                    return;
                }
                WorldPacket packet(CMSG_SPELLCLICK, 8);
                packet << well->GetObjectGuid();
                SpellHandlers::HandleSpellClick(*SessionOf(*ally), packet);
                well = Well(p);
                reads["charges"] = WellCharges(well);
                reads["renew"] = RenewCaster(p, ally);
                reads["renewCharges"] = RenewCharges(ally);
                SpellAuraHolder* holder = ally->GetSpellAuraHolder(kRenew);
                Aura* aura = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                reads["base"] = aura ? int64(aura->GetTickAmountBeforeDone()) : kNone;
                reads["amount"] = aura ? int64(aura->GetModifier()->m_amount) : kNone;
                m_rec.Note("the Lightwell's charges " + Shown(reads["charges"]) + "; the friend's " + U(kRenew) + ": " +
                           HolderOf(ally, kRenew) + ", its tick " + Shown(reads["amount"]) + " (" + Shown(reads["base"]) +
                           " before the done side), its caster " + CasterText(reads["renew"]));
                ReadCombat(reads, p, well);
            }

            /// The Camel's hit on the friend of `percent` percent of his maximum health, the proc call a melee hit makes
            /// on its victim (Unit::ProcDamageAndSpell with the hit's flags, a normal hit and the damage); answers the
            /// damage.
            int64 Hit(Player* ally, Creature* camel, uint32 percent)
            {
                const uint32 damage = uint32(uint64(ally->GetMaxHealth()) * percent / 100);
                camel->ProcDamageAndSpell(ally, PROC_FLAG_SUCCESSFUL_MELEE_HIT,
                                          PROC_FLAG_TAKEN_MELEE_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE, PROC_EX_NORMAL_HIT,
                                          damage, BASE_ATTACK, NULL, 0);
                m_rec.Note("the Camel's normal melee hit of " + U(damage) + " (" + U(percent) + "% of the friend's " +
                           U(ally->GetMaxHealth()) + ") reaches the friend's procs; his " + U(kRenew) + " right after: " +
                           HolderOf(ally, kRenew));
                return int64(damage);
            }

            /// Whose the friend's 7001 is: 1 when his caster is the priest, 0 for another caster, kNone for none.
            static int64 RenewCaster(Player* p, Player* ally)
            {
                SpellAuraHolder* holder = ally ? ally->GetSpellAuraHolder(kRenew) : NULL;
                return !holder ? kNone : holder->GetCasterGuid() == p->GetObjectGuid() ? 1 : 0;
            }

            /// The proc charges of the friend's 7001; kNone for none.
            static int64 RenewCharges(Player* ally)
            {
                SpellAuraHolder* holder = ally ? ally->GetSpellAuraHolder(kRenew) : NULL;
                return holder ? int64(holder->GetAuraCharges()) : kNone;
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

            /// `value` as text; `none` for kNone, `unread` for kUnread.
            static std::string Shown(int64 value)
            {
                return value == kNone ? std::string("none")
                                      : value == kUnread ? std::string("unread") : std::to_string(value);
            }

            /// A Renew's caster as the record shows it.
            static std::string CasterText(int64 value)
            {
                return value == 1 ? std::string("him") : value == kNone ? std::string("none") : std::string("other");
            }
        };
    }

    void RegisterPriestLightwellScenarios(Runner& r)
    {
        r.Register(new PriestLightwell());
    }
}
