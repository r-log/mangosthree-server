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
#include "Group.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

/**
 * @file ScenariosPriestDivineHymn.cpp
 * @brief The priest's heal rule scenarios: order 1220, Divine Hymn's heal picks as many of his group's members as its
 * channel's effect 1 names and acts as a spell heal: his healing-done bonus, Spiritual Healing and Heavenly Voice, his
 * Grace on the healed unit, and a crit roll Holy Word: Serenity's crit aura raises; order 1221, Holy Word: Serenity's
 * crit aura raises the crit chance of his own heals on its holder and of nothing else.
 */

namespace Harness
{
    namespace
    {
        uint32 const kImprovedShieldTalent = 10736;
        uint32 const kImprovedShieldRankIndex = 1;

        /// What ReadOwn kept of step k under `name`; -2 when the step was not read.
        template <class State>
        int64 KeptOf(State const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -2;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }

        /// The value of effect `index` of `spell` in the client's data; 0 when the store lacks the spell.
        int64 DataValueOf(uint32 spell, SpellEffectIndex index)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spell);
            return info ? int64(info->CalculateSimpleValue(index)) : 0;
        }

        std::string Shown(int64 value)
        {
            return value == -2 ? std::string("unread") : std::to_string(value);
        }

        uint32 const kCritRolls = 2000;
        int64 const kCritBonusCeiling = 60;

        /// The crit chance Unit::IsSpellCrit computes for `p`'s `spellId` at `target` as a hit of `type`, above his
        /// crit sheet for the spell's school, in whole percent: the least b of 0 to kCritBonusCeiling at which
        /// kCritRolls rolls with that sheet set to -b by call never crit. kCritBonusCeiling + 1 when every b crit; -1
        /// when the store lacks the spell. The sheet is put back as it was.
        int64 CritBonusOf(Player* p, Unit* target, uint32 spellId, DamageEffectType type)
        {
            SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
            if (!spell)
            {
                return -1;
            }
            const SpellSchoolMask mask = GetSpellSchoolMask(spell);
            const uint16 field = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(mask));
            const float sheet = p->GetFloatValue(field);
            int64 bonus = kCritBonusCeiling + 1;
            for (int64 b = 0; b <= kCritBonusCeiling && bonus > kCritBonusCeiling; ++b)
            {
                p->SetFloatValue(field, -float(b));
                bool crit = false;
                for (uint32 roll = 0; roll < kCritRolls && !crit; ++roll)
                {
                    crit = p->IsSpellCrit(target, spell, mask, BASE_ATTACK, type);
                }
                if (!crit)
                {
                    bonus = b;
                }
            }
            p->SetFloatValue(field, sheet);
            return bonus;
        }

        /// 1 when `p`'s `spellId` at `target` as a heal crits within kCritRolls rolls with his crit sheet for its
        /// school set to 100 by call, else 0; -1 when the store lacks the spell. The sheet is put back as it was.
        int64 CritsAtFullSheet(Player* p, Unit* target, uint32 spellId)
        {
            SpellEntry const* spell = sSpellStore.LookupEntry(spellId);
            if (!spell)
            {
                return -1;
            }
            const SpellSchoolMask mask = GetSpellSchoolMask(spell);
            const uint16 field = uint16(PLAYER_SPELL_CRIT_PERCENTAGE1 + GetFirstSchoolInMask(mask));
            const float sheet = p->GetFloatValue(field);
            p->SetFloatValue(field, 100.0f);
            bool crit = false;
            for (uint32 roll = 0; roll < kCritRolls && !crit; ++roll)
            {
                crit = p->IsSpellCrit(target, spell, mask, BASE_ATTACK, HEAL);
            }
            p->SetFloatValue(field, sheet);
            return crit ? 1 : 0;
        }

        /**
         * S1220 `priest-divine-hymn`: Divine Hymn (64843) channelled by him heals, each tick, through 64844, as many
         * of his group's members within its reach as the channel's effect 1 names, the lowest in health first, him
         * included, and its heal acts as a spell heal: it takes his healing-done bonus (his spell power at 64844's
         * coefficient, then Spiritual Healing's (87336) and Heavenly Voice's (87431) percents), the
         * healing-received aura he put on the healed unit (Grace, 77613) and rolls its crit, which Holy Word:
         * Serenity's crit aura (88684's effect 1) raises.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree;
         * Divine Hymn, Spiritual Healing and Heavenly Voice's second rank learned; kSpellPower spell power; the Camel
         * ten yards in front of him, unused. Once he is staged, kFriends more harness players, human priests taken to
         * level 85 with that level's stats, are spawned in a line kFriendsBehind yards behind him, and he and they are
         * put in one group: each linked to an in-memory Group by Player::SetGroup, the reference list the raid and
         * party target fill walks (Spell::FillRaidOrPartyTargets), with no group row and no member row, since the
         * harness never writes to the character database. Last, Increased Critical Spell (23434) is put on him at
         * -kCritAuraPoints, so none of the channel's heals crits. Steps:
         *  1. Divine Hymn cast by him at himself, triggered: an 8 second channel whose effect 0 casts 64844 every 2
         *     seconds. Right after it, before the first tick, every friend's health is set to half its maximum, and
         *     the data and his bonuses are read; then, at the first friend, by call: the crit chance
         *     Unit::IsSpellCrit computes for his 64844 as a heal above his crit sheet (the 1221 read), with his 88684
         *     put on the friend and with none; whether it crits with his sheet at 100; and what
         *     Unit::SpellHealingBonusTaken makes of kTakenBase of his 64844 with his Grace on the friend and with the
         *     second friend's; each aura taken off after its read;
         *  2. a wait, after the channel's end: he and his friends leave the group, which is deleted.
         *
         * `divineHymn`: `targets=<the units each SMSG_SPELL_GO of 64844 in step 1's windows hits, per GO>,heals=<the
         * SMSG_SPELLHEALLOG packets of 64844 he sent after each of those GOs>,first=<the distinct heals of the logs
         * that are a unit's first 64844 heal in the step, joined by +>,again=<the distinct heals of the later ones; -
         * for none>,crit=<his 64844's crit chance with his 88684 on the friend>/<with none>,rolls=<1 when it crits at
         * his sheet at 100, else 0>,grace=<kTakenBase of it taken with his Grace>/<with the other friend's>`. With C =
         * 64843's effect 1 value, B = his 64844 effect 0 base (Unit::CalculateSpellDamage), S = his healing spell
         * power (Unit::SpellBaseHealingBonusDone), P = Unit::SpellBonusWithCoeffs of S for 64844,
         * M = what his spell modifiers of SPELLMOD_DAMAGE make of 100 for 64844, D = B + P with his spell modifiers
         * of SPELLMOD_DAMAGE applied (as Unit::SpellHealingBonusDone applies them) and A = D under 64844's effect 1
         * value as the healing-taken percent of a unit that holds 64844 from an earlier tick: OK when kFriends
         * friends were spawned and grouped with him, C, B and S are positive, 64844's coefficient is 0.429, P is
         * within 1 of S x 0.429, M is 100 + 87336's effect 0 value + 87431's effect 0 value, he holds no
         * healing-done percent aura, at least one GO came, every GO hits C units and is followed by C heal logs, and
         * every first heal is D and every later one A, and at crit=V/0,rolls=1,grace=G/kTakenBase with V = 88684's
         * effect 1 value and G = kTakenBase x (100 + 77613's effect 0 value) / 100.
         */
        class PriestDivineHymn : public StagedCasterScenario
        {
        public:
            PriestDivineHymn() : StagedCasterScenario("priest-divine-hymn", 1220, "divineHymn", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }
            std::vector<uint32> Spells() const override { return { kDivineHymn, kSpiritualHealing, kHeavenlyVoice2 }; }
            int32 SpellPower() const override { return kSpellPower; }
            bool Heals() const override { return true; }

            void AfterStage(Player& p) override
            {
                int32 noCrit = -kCritAuraPoints;
                p.CastCustomSpell(&p, kCritAura, &noCrit, NULL, NULL, true);
                m_friends.clear();
                m_group.reset(new Group());
                p.SetGroup(m_group.get(), 0);
                for (uint32 i = 0; i < kFriends; ++i)
                {
                    const float x = P0.x - kFriendsBehind;
                    const float y = P0.y + (float(i) - float(kFriends - 1) / 2.0f) * kFriendSpacing;
                    if (Player* friendPlayer = SpawnPlayer(x, y, Ground(x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                    {
                        friendPlayer->SetLevel(kFriendLevel);
                        friendPlayer->InitStatsForLevel(true);
                        friendPlayer->SetGroup(m_group.get(), i < kFirstSubGroupFriends ? 0 : 1);
                        m_friends.push_back(friendPlayer->GetObjectGuid());
                    }
                }
            }

            std::vector<Step> Steps() const override
            {
                return { SelfAura(kDivineHymn, kChannelStepMs), Wait(kWaitMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 64843: 72 fields, fnv 2389eac5
                    { 64843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 64844 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 20 }, { "eff2.EffectAuraPeriod", 2000 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                    // 64844: 49 fields, fnv 8ce2a872
                    { 64844, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 3023 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 319 }, { "eff0.EffectRadiusIndex_1", 23 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 30 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 118 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 127 }, { "eff1.EffectRadiusIndex_1", 23 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 30 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87336: 49 fields, fnv 1f3d7efd
                    { 87336, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 576742382312103424 }, { "eff0.EffectSpellClassMask.Flags2", 2129924 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 22 }, { "eff1.EffectSpellClassMask.Flags", 10485824 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 87431: 49 fields, fnv 03e9f777
                    { 87431, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags2", 4 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", -300000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.EffectSpellClassMask.Flags", 18014398509481984 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77613: 26 fields, fnv 48586316
                    { 77613, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 283 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 281630019427905 }, { "eff0.EffectSpellClassMask.Flags2", 2147533828 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 88684: 49 fields, fnv 955f0e19
                    { 88684, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1056494846 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 308 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 17582530048 }, { "eff1.EffectSpellClassMask.Flags2", 4 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 23434: 26 fields, fnv 6bb46cc2
                    { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
            {
                if (k >= kSteps)
                {
                    return;
                }
                Seen& seen = st.seen[k];
                if (k == kChannelStep && !atRead)
                {
                    std::string healths;
                    for (size_t i = 0; i < m_friends.size(); ++i)
                    {
                        if (Player* friendPlayer = sPlayerRegistry.Find(m_friends[i]))
                        {
                            friendPlayer->SetHealth(friendPlayer->GetMaxHealth() / 2);
                            healths += (healths.empty() ? "" : ", ") + U(friendPlayer->GetHealth()) + " of " +
                                       U(friendPlayer->GetMaxHealth());
                        }
                    }
                    ReadTheData(seen, p);
                    m_rec.Note("his friends' health set to half: " + (healths.empty() ? std::string("none") : healths) +
                               "; his health " + U(p->GetHealth()) + " of " + U(p->GetMaxHealth()));
                    return;
                }
                if (k == kWaitStep && atRead)
                {
                    LeaveTheGroup(p);
                }
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 count = KeptOf(st, kChannelStep, "count");
                const int64 friends = KeptOf(st, kChannelStep, "friends");
                const int64 grouped = KeptOf(st, kChannelStep, "grouped");
                const int64 base = KeptOf(st, kChannelStep, "base");
                const int64 power = KeptOf(st, kChannelStep, "power");
                const int64 part = KeptOf(st, kChannelStep, "part");
                const int64 coefficientMilli = KeptOf(st, kChannelStep, "coefficientMilli");
                const int64 percent = KeptOf(st, kChannelStep, "percent");
                const int64 healing = KeptOf(st, kChannelStep, "spiritualHealing");
                const int64 voice = KeptOf(st, kChannelStep, "heavenlyVoice");
                const int64 taken = KeptOf(st, kChannelStep, "taken");
                const int64 donePercentAuras = KeptOf(st, kChannelStep, "donePercentAuras");
                const int64 wanted = KeptOf(st, kChannelStep, "wanted");
                const int64 again = wanted >= 0 ?
                    int64(int32(int32(wanted) * Combat::SpellHealingTakenPercent(0, int32(taken)))) : -1;
                const int64 partWanted = power * kCoefficientMilli / 1000;
                const int64 serenity = KeptOf(st, kChannelStep, "serenity");
                const int64 critOwn = KeptOf(st, kChannelStep, "critOwn");
                const int64 critNone = KeptOf(st, kChannelStep, "critNone");
                const int64 rolls = KeptOf(st, kChannelStep, "rolls");
                const int64 grace = KeptOf(st, kChannelStep, "grace");
                const int64 graceOwn = KeptOf(st, kChannelStep, "graceOwn");
                const int64 graceOther = KeptOf(st, kChannelStep, "graceOther");
                const int64 graceWanted = int64(kTakenBase) * (100 + grace) / 100;

                bool ok = count > 0 && friends == int64(kFriends) && grouped == int64(kFriends) + 1 && base > 0 &&
                          power > 0 && coefficientMilli == kCoefficientMilli && part >= partWanted - 1 &&
                          part <= partWanted + 1 && healing > 0 && voice > 0 && percent == 100 + healing + voice &&
                          donePercentAuras == 0 && wanted > 0 && serenity > 0 && critOwn == serenity &&
                          critNone == 0 && rolls == 1 && grace > 0 && graceOwn == graceWanted &&
                          graceOther == int64(kTakenBase);

                std::vector<Tick> ticks = TicksOf(st.player.GetRawValue());
                ok = ok && !ticks.empty();
                std::string targets, heals;
                std::set<uint64> healed;
                std::set<uint32> firsts, agains;
                for (size_t t = 0; t < ticks.size(); ++t)
                {
                    Tick const& tick = ticks[t];
                    ok = ok && tick.hits == count && int64(tick.heals.size()) == tick.hits;
                    for (size_t h = 0; h < tick.heals.size(); ++h)
                    {
                        const bool first = healed.insert(tick.heals[h].first).second;
                        const uint32 amount = tick.heals[h].second;
                        (first ? firsts : agains).insert(amount);
                        ok = ok && int64(amount) == (first ? wanted : again);
                    }
                    targets += (t ? "/" : "") + std::to_string(tick.hits);
                    heals += (t ? "/" : "") + U(uint32(tick.heals.size()));
                }

                const std::string value = "targets=" + (targets.empty() ? std::string("-") : targets) + ",heals=" +
                                          (heals.empty() ? std::string("-") : heals) + ",first=" + Joined(firsts) +
                                          ",again=" + Joined(agains) + ",crit=" + Shown(critOwn) + "/" +
                                          Shown(critNone) + ",rolls=" + Shown(rolls) + ",grace=" + Shown(graceOwn) +
                                          "/" + Shown(graceOther);
                const std::string detail =
                    "wanted " + Shown(count) + " units a tick (" + U(kDivineHymn) + "'s effect 1 value) of " +
                    U(kFriends) + " friends at half health and him at his maximum, " + Shown(friends) +
                    " spawned and " + Shown(grouped) + " grouped with him; each " + U(kHymnHeal) + " heal (" +
                    Shown(base) + " base + " + Shown(part) + " for his spell power " + Shown(power) + " at " +
                    Shown(coefficientMilli) + "/1000, wanted " + std::to_string(kCoefficientMilli) + "/1000) x " +
                    Shown(percent) + "/100 (wanted 100 + " + Shown(healing) + " for " + U(kSpiritualHealing) + " + " +
                    Shown(voice) + " for " + U(kHeavenlyVoice2) + "), " + Shown(wanted) +
                    " at a unit's first heal and " + std::to_string(again) + " under " + U(kHymnHeal) + "'s +" +
                    Shown(taken) + "% healing taken after; his healing-done percent auras " +
                    Shown(donePercentAuras) + ", wanted 0; at the first friend, his " + U(kHymnHeal) +
                    "'s crit chance raised by " + Shown(serenity) + " (" + U(kSerenity) + "'s effect 1 value) with " +
                    "his " + U(kSerenity) + " on the friend and by nothing with none, a crit with his sheet at 100, " +
                    "and " + U(kTakenBase) + " of it taken as " + std::to_string(graceWanted) + " with his " +
                    U(kGrace) + " (+" + Shown(grace) + "%) and as " + U(kTakenBase) + " with the other friend's";
                return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kDivineHymn = 64843;
            static constexpr uint32 kHymnHeal = 64844;
            static constexpr uint32 kSpiritualHealing = 87336;
            static constexpr uint32 kHeavenlyVoice2 = 87431;
            static constexpr uint32 kSerenity = 88684;
            static constexpr uint32 kGrace = 77613;
            static constexpr uint32 kTakenBase = 1000;
            static constexpr int32 kSpellPower = 5000;
            static constexpr int64 kCoefficientMilli = 429;
            static constexpr uint32 kFriends = 6;
            static constexpr uint32 kFirstSubGroupFriends = 4;
            static constexpr uint32 kFriendLevel = 85;
            static constexpr float kFriendsBehind = 4.0f;
            static constexpr float kFriendSpacing = 3.0f;
            static constexpr uint32 kChannelStepMs = 9000;
            static constexpr uint32 kWaitMs = 1000;
            static constexpr size_t kSteps = 2;
            static constexpr uint32 kChannelStep = 0;
            static constexpr uint32 kWaitStep = 1;

            std::vector<ObjectGuid> m_friends;  ///< the friends, spawned after the stage
            std::unique_ptr<Group> m_group;     ///< the group he and his friends are linked to; never stored

            /// One SMSG_SPELL_GO of 64844 and the 64844 heal logs he sent after it, before the next: each heal's
            /// unit and amount.
            struct Tick
            {
                int64 hits = 0;
                std::vector<std::pair<uint64, uint32> > heals;
            };

            /// After the cast: the data's values and his bonuses for 64844, kept in `seen`, and noted.
            void ReadTheData(Seen& seen, Player* p)
            {
                SpellEntry const* heal = sSpellStore.LookupEntry(kHymnHeal);
                SpellEffectEntry const* healEffect = heal ? heal->GetSpellEffect(EFFECT_INDEX_0) : NULL;
                Player* someFriend = m_friends.empty() ? NULL : sPlayerRegistry.Find(m_friends[0]);
                uint32 grouped = 0;
                if (Group* group = p->GetGroup())
                {
                    for (GroupReference* itr = group->GetFirstMember(); itr != NULL; itr = itr->next())
                    {
                        grouped += itr->getSource() ? 1 : 0;
                    }
                }
                seen.reads["count"] = DataValueOf(kDivineHymn, EFFECT_INDEX_1);
                seen.reads["friends"] = int64(m_friends.size());
                seen.reads["grouped"] = grouped;
                seen.reads["spiritualHealing"] = DataValueOf(kSpiritualHealing, EFFECT_INDEX_0);
                seen.reads["heavenlyVoice"] = DataValueOf(kHeavenlyVoice2, EFFECT_INDEX_0);
                seen.reads["taken"] = DataValueOf(kHymnHeal, EFFECT_INDEX_1);
                seen.reads["coefficientMilli"] =
                    healEffect ? int64(healEffect->EffectBonusCoefficient * 1000.0f + 0.5f) : 0;
                seen.reads["donePercentAuras"] = int64(p->GetAurasByType(SPELL_AURA_MOD_HEALING_DONE_PERCENT).size());
                if (!heal)
                {
                    m_rec.Note("the store lacks " + U(kHymnHeal));
                    return;
                }
                const int32 power = p->SpellBaseHealingBonusDone(GetSpellSchoolMask(heal));
                const int32 base = p->CalculateSpellDamage(someFriend ? static_cast<Unit*>(someFriend) : p, heal,
                                                          EFFECT_INDEX_0);
                const int32 part = p->SpellBonusWithCoeffs(heal, 0, power, 0, HEAL, true, SPELL_BONUS_KIND_HEAL);
                float percent = 100.0f;
                float wanted = float(base + part);
                if (SpellModMgr* mods = p->GetSpellMods())
                {
                    mods->ApplySpellMod(kHymnHeal, SPELLMOD_DAMAGE, percent);
                    mods->ApplySpellMod(kHymnHeal, SPELLMOD_DAMAGE, wanted);
                }
                seen.reads["power"] = power;
                seen.reads["base"] = base;
                seen.reads["part"] = part;
                seen.reads["percent"] = int64(percent + 0.5f);
                seen.reads["wanted"] = int64(uint32(wanted));
                m_rec.Note(U(kDivineHymn) + "'s effect 1 " + std::to_string(seen.reads["count"]) + "; " +
                           std::to_string(grouped) + " units in his group, " + U(uint32(m_friends.size())) +
                           " friends; " + U(kHymnHeal) + ": base " + std::to_string(base) + ", coefficient " +
                           std::to_string(seen.reads["coefficientMilli"]) + "/1000, his healing spell power " +
                           std::to_string(power) + ", its part " + std::to_string(part) + ", his modifiers " +
                           std::to_string(seen.reads["percent"]) + "/100 (" + U(kSpiritualHealing) + " " +
                           std::to_string(seen.reads["spiritualHealing"]) + ", " + U(kHeavenlyVoice2) + " " +
                           std::to_string(seen.reads["heavenlyVoice"]) + "), the done heal " +
                           std::to_string(seen.reads["wanted"]) + "; its effect 1 healing taken +" +
                           std::to_string(seen.reads["taken"]) + "%; his healing-done percent auras " +
                           std::to_string(seen.reads["donePercentAuras"]));
                ReadTheSpellHealRules(seen, p, heal, someFriend);
            }

            /// After the cast, before the first tick, at the first friend, by call: the crit chance of his 64844 with
            /// his 88684 on the friend and with none, whether it crits with his sheet at 100, and what the friend
            /// takes of kTakenBase of it with his Grace on the friend and with the second friend's; each aura taken
            /// off after its read. Kept in `seen`, and noted.
            void ReadTheSpellHealRules(Seen& seen, Player* p, SpellEntry const* heal, Player* healed)
            {
                Player* otherFriend = m_friends.size() > 1 ? sPlayerRegistry.Find(m_friends[1]) : NULL;
                seen.reads["serenity"] = DataValueOf(kSerenity, EFFECT_INDEX_1);
                seen.reads["grace"] = DataValueOf(kGrace, EFFECT_INDEX_0);
                if (!healed || !otherFriend)
                {
                    m_rec.Note("the friends the spell heal reads need were not spawned");
                    return;
                }
                PutAura(healed, kSerenity, p);
                seen.reads["critOwn"] = CritBonusOf(p, healed, kHymnHeal, HEAL);
                const std::string serenityHolder = HolderOf(healed, kSerenity);
                healed->RemoveAurasDueToSpell(kSerenity);
                seen.reads["critNone"] = CritBonusOf(p, healed, kHymnHeal, HEAL);
                seen.reads["rolls"] = CritsAtFullSheet(p, healed, kHymnHeal);
                PutAura(healed, kGrace, p);
                seen.reads["graceOwn"] = int64(healed->SpellHealingBonusTaken(p, heal, int32(kTakenBase), HEAL, 1));
                const std::string graceHolder = HolderOf(healed, kGrace);
                healed->RemoveAurasDueToSpell(kGrace);
                PutAura(healed, kGrace, otherFriend);
                seen.reads["graceOther"] = int64(healed->SpellHealingBonusTaken(p, heal, int32(kTakenBase), HEAL, 1));
                healed->RemoveAurasDueToSpell(kGrace);
                m_rec.Note("at the first friend: his " + U(kHymnHeal) + "'s crit chance +" +
                           std::to_string(seen.reads["critOwn"]) + " with his " + U(kSerenity) + " (" +
                           serenityHolder + "), +" + std::to_string(seen.reads["critNone"]) +
                           " with none, crits at his sheet at 100: " + std::to_string(seen.reads["rolls"]) + "; " +
                           U(kTakenBase) + " of it taken as " + std::to_string(seen.reads["graceOwn"]) + " with his " +
                           U(kGrace) + " (" + graceHolder + "), " + std::to_string(seen.reads["graceOther"]) +
                           " with the other friend's");
            }

            /// He and his friends leave the group, which is deleted.
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
                for (size_t i = 0; i < m_friends.size(); ++i)
                {
                    Player* friendPlayer = sPlayerRegistry.Find(m_friends[i]);
                    if (friendPlayer && friendPlayer->GetGroup() == m_group.get())
                    {
                        friendPlayer->SetGroup(NULL);
                    }
                }
                m_group.reset();
                m_rec.Note("he and his friends left the group");
            }

            /// The SMSG_SPELL_GO packets of 64844 in step 1's windows, in the order they were sent, each with the
            /// units its hit list names and the 64844 heal logs of caster `self` sent after it and before the next.
            std::vector<Tick> TicksOf(uint64 self) const
            {
                std::vector<Tick> ticks;
                const std::vector<std::string> windows = StepWindows(kChannelStep);
                std::vector<Recorder::Seen> const& packets = m_rec.Packets();
                for (size_t i = 0; i < packets.size(); ++i)
                {
                    Recorder::Seen const& packet = packets[i];
                    if (packet.payload.empty() ||
                        std::find(windows.begin(), windows.end(), packet.window) == windows.end())
                    {
                        continue;
                    }
                    if (packet.opcode == SMSG_SPELL_GO)
                    {
                        std::string text;
                        uint32 id = 0;
                        if (Trace::DecodeSpellCast(true, &packet.payload[0], packet.payload.size(), m_rec.Roles(),
                                                   text) &&
                            sscanf(text.c_str(), "spell=%u", &id) == 1 && id == kHymnHeal)
                        {
                            Tick tick;
                            tick.hits = HitsOf(text);
                            ticks.push_back(tick);
                        }
                    }
                    else if (packet.opcode == SMSG_SPELLHEALLOG && !ticks.empty())
                    {
                        Trace::SpellHealLog log;
                        if (Trace::ReadSpellHealLog(&packet.payload[0], packet.payload.size(), log) &&
                            log.caster == self && log.spell == kHymnHeal)
                        {
                            ticks.back().heals.push_back(std::make_pair(log.target, log.heal));
                        }
                    }
                }
                return ticks;
            }

            /// How many units a decoded SMSG_SPELL_GO names in its hit list.
            static int64 HitsOf(std::string const& go)
            {
                static char const* const kList = " hit=[";
                const size_t open = go.find(kList);
                if (open == std::string::npos)
                {
                    return 0;
                }
                const size_t from = open + strlen(kList);
                const size_t close = go.find(']', from);
                if (close == std::string::npos || close == from)
                {
                    return 0;
                }
                int64 units = 1;
                for (size_t at = from; at < close; ++at)
                {
                    units += go[at] == ',' ? 1 : 0;
                }
                return units;
            }

            /// The amounts, joined by +; - for none.
            static std::string Joined(std::set<uint32> const& amounts)
            {
                std::string out;
                for (std::set<uint32>::const_iterator i = amounts.begin(); i != amounts.end(); ++i)
                {
                    out += (out.empty() ? "" : "+") + U(*i);
                }
                return out.empty() ? std::string("-") : out;
            }
        };

        /**
         * S1221 `priest-crit-for-caster`: Holy Word: Serenity's crit aura (88684's effect 1,
         * SPELL_AURA_MOD_CRIT_FROM_CASTER, its effect 1 value 25 in the client's data) raises the crit chance of the
         * heals its caster casts at its holder whose class mask fits its own, and nothing else.
         * Staged: a level 85 human priest (StagedCasterScenario with CLASS_PRIEST) with Improved Power Word: Shield 2
         * (talent 10736, rank index 1, spell 14768) as his first talent, which makes Discipline his primary tree; the
         * Camel ten yards in front of him; after the stage a second harness player, a human priest taken to level 85,
         * kOtherSide yards to his side. Steps, each a wait read right after it acts:
         *  1. no aura on the Camel;
         *  2. 88684's aura put on the Camel as the GM aura command puts it (its effect 1 alone, effect 0 being a heal),
         *     with him as its caster, and taken off after the reads;
         *  3. the same with the other priest as its caster.
         * Each crit read is the crit chance Unit::IsSpellCrit computes for his spell at the Camel, a heal for Flash
         * Heal (2061) and a direct hit for Smite (585), above his crit sheet for its school, in whole percent: the
         * least b at which kCritRolls rolls with the sheet set to -b by call never crit, the sheet put back after.
         *
         * `critFromCaster`: `crit=<Flash Heal at step 2>/<Smite at step 2>/<Flash Heal at step 3>,base=<Flash Heal at
         * step 1>`. OK at crit=V/0/0,base=0 with V = 88684's effect 1 value, positive.
         */
        class PriestCritFromCaster : public StagedCasterScenario
        {
        public:
            PriestCritFromCaster()
                : StagedCasterScenario("priest-crit-for-caster", 1221, "critFromCaster", CLASS_PRIEST) {}

        protected:
            std::vector<std::pair<uint32, uint32> > Talents() const override
            {
                return { { kImprovedShieldTalent, kImprovedShieldRankIndex } };
            }

            void AfterStage(Player& /*p*/) override
            {
                m_other = ObjectGuid();
                const float y = P0.y + kOtherSide;
                if (Player* other = SpawnPlayer(P0.x, y, Ground(P0.x, y, P0.z), PlayerFacing(), CLASS_PRIEST))
                {
                    m_other = other->GetObjectGuid();
                    other->SetLevel(kOtherLevel);
                    other->InitStatsForLevel(true);
                }
            }

            std::vector<Step> Steps() const override
            {
                return { Wait(kStepMs), Wait(kStepMs), Wait(kStepMs) };
            }

            std::vector<SpellPrint> Prints() const override
            {
                return {
                    // 14768: 49 fields, fnv ffd3938f
                    { 14768, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 20 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 8 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 30 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 77484: 49 fields, fnv cac7b251
                    { 77484, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBonusCoefficient", 1075838976 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 2 }, { "eff0.EffectSpellClassMask.Flags", 33554432 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 250 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 88684: 49 fields, fnv 955f0e19
                    { 88684, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1056494846 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 308 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 17582530048 }, { "eff1.EffectSpellClassMask.Flags2", 4 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                    // 2061: 26 fields, fnv b42e3764
                    { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                    // 585: 26 fields, fnv 47a76d3f
                    { 585, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectBonusCoefficient", 1062937297 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 5 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                };
            }

            void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
            {
                if (atRead || k >= kSteps)
                {
                    return;
                }
                std::map<std::string, int64>& reads = st.seen[k].reads;
                Player* other = m_other ? sPlayerRegistry.Find(m_other) : NULL;
                std::string note = "his holy crit sheet " +
                                   std::to_string(p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_HOLY));
                switch (k)
                {
                    case 0:
                        reads["flash"] = CritBonusOf(p, target, kFlashHeal, HEAL);
                        note += "; no aura on the Camel: Flash Heal +" + Shown(reads["flash"]);
                        break;
                    case 1:
                        PutAura(target, kSerenity, p);
                        reads["flash"] = CritBonusOf(p, target, kFlashHeal, HEAL);
                        reads["smite"] = CritBonusOf(p, target, kSmite, SPELL_DIRECT_DAMAGE);
                        reads["value"] = DataValueOf(kSerenity, EFFECT_INDEX_1);
                        note += "; his " + U(kSerenity) + " on the Camel (" + HolderOf(target, kSerenity) +
                                "): Flash Heal +" + Shown(reads["flash"]) + ", Smite +" + Shown(reads["smite"]);
                        target->RemoveAurasDueToSpell(kSerenity);
                        break;
                    default:
                        if (other)
                        {
                            PutAura(target, kSerenity, other);
                        }
                        reads["flash"] = CritBonusOf(p, target, kFlashHeal, HEAL);
                        note += "; the other priest's " + U(kSerenity) + " on the Camel (" +
                                HolderOf(target, kSerenity) + "): Flash Heal +" + Shown(reads["flash"]);
                        target->RemoveAurasDueToSpell(kSerenity);
                        break;
                }
                m_rec.Note(note);
            }

            std::string Own(St const& st) const override
            {
                if (st.steps.size() != kSteps)
                {
                    return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
                }
                const int64 value = KeptOf(st, 1, "value");
                const int64 own = KeptOf(st, 1, "flash");
                const int64 smite = KeptOf(st, 1, "smite");
                const int64 others = KeptOf(st, 2, "flash");
                const int64 base = KeptOf(st, 0, "flash");
                const bool ok = value > 0 && own == value && smite == 0 && others == 0 && base == 0;
                const std::string line = "crit=" + Shown(own) + "/" + Shown(smite) + "/" + Shown(others) + ",base=" +
                                         Shown(base);
                const std::string detail =
                    "wanted his Flash Heal's crit chance at the Camel raised by " + Shown(value) + " (" + U(kSerenity) +
                    "'s effect 1 value) with his " + U(kSerenity) + " on it, his Smite's by nothing, his Flash " +
                    "Heal's by nothing with the other priest's, and by nothing with none";
                return line + (ok ? "(OK: " : "(BUG: ") + detail + ")";
            }

        private:
            static constexpr uint32 kSerenity = 88684;
            static constexpr uint32 kFlashHeal = 2061;
            static constexpr uint32 kSmite = 585;
            static constexpr uint32 kOtherLevel = 85;
            static constexpr float kOtherSide = 4.0f;
            static constexpr uint32 kStepMs = 1000;
            static constexpr size_t kSteps = 3;

            ObjectGuid m_other;                 ///< the other priest, spawned after the stage
        };
    }

    void RegisterPriestDivineHymnScenarios(Runner& r)
    {
        r.Register(new PriestDivineHymn());
        r.Register(new PriestCritFromCaster());
    }
}
