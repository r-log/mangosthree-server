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

#include "Scenario.h"
#include "Harness.h"
#include "RecordedScenario.h"
#include "SpellRecorder.h"
#include "QuestFixture.h"
#include "SpellScenario.h"
#include "StagedCasterScenario.h"
#include "Creature.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "ScriptMgr.h"
#include "spells/pipeline/Spell.h"
#include "spells/pipeline/SpellRules.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "combat/HitInheritance.h"
#include "spells/SpellScriptHooks.h"
#include "spells/handlers/SpellHandlerRegistry.h"
#include "combat/SpellBonus.h"
#include "data/GameConfig.h"
#include "DBCStores.h"
#include "Opcodes.h"
#include "Pet.h"
#include "UpdateData.h"
#include "HarnessAI.h"
#include "WorldSession.h"
#include "WorldPacket.h"
#include "Map.h"
#include "MapManager.h"
#include "MapPhase.h"
#include "maps/CellImpl.h"
#include "maps/GridNotifiers.h"
#include "maps/GridNotifiersImpl.h"
#include "movement/MoveSpline.h"
#include "session/handlers/entities/MovementHandlers.h"
#include "session/handlers/entities/PetHandlers.h"
#include "session/handlers/spells/SpellHandlers.h"
#include "session/packets/spells/CastTargetsPacket.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

// Part of the spell family (ScenariosSpell.cpp, whose file comment numbers its scenarios, orders 930 to 1199):
// the Fire mage's scenarios, and the mage scenarios that derive from a Fire base. Each registers through
// RegisterMageFireScenarios, which the runner calls from the scenario file list (HarnessScenarioFiles.inc).
namespace Harness
{
    /**
     * S950 `mage-fire-fireball-crit`: a human mage staged to level 85 with 1000 spell power and no talent or
     * glyph (Scenario::StagePlayer) casts Fireball (133) kFireballCasts times through the CAST_SPELL handler's
     * calls at a Camel (50249) that Scenario::SpawnTarget puts at level 85 ten yards in front of him. One cast
     * every kCastEveryMs, his mana set to its maximum by call before each; each hit is read kReadAfterMs after
     * its cast (a 2.5 s cast, then ten yards at Fireball's 24 yd/s), and that read window closes when the next
     * cast opens its own.
     *
     * Fireball qualifies: one SpellEffect row, SCHOOL_DAMAGE at A=6 and B=0, SPELLFAMILY_MAGE, nothing
     * triggered, no script binding. Its effect holds the client's spell power coefficient (EffectBonusCoefficient
     * 1.236), the coefficient the server applies at level 85. Every mage is created with two
     * proc passives, Offensive State (79684, which casts Arcane Missiles! 79683 on him) and Deep Freeze Immunity
     * State (71761, which names Deep Freeze 71757): all four are fingerprinted, their procs are recorded as they
     * happen, and a cast is refused when a holder outside the fingerprinted spells could proc. The Camel is level
     * 85 in its own template, so its level-85 health comes from the expansion-3 stats row (77490), more than five
     * crits take; it runs no AI and binds no script.
     *
     * THE SEED. Each cast's hit roll, crit roll and base points are drawn in a map update, under the map hook's
     * RNG::Seed(TickSeed(base, 950, elapsed)), so each is a function of the cast's elapsed time alone and every
     * run draws the same ones. Under the harness's seed base, kCastEveryMs puts all five rolls on hits; none
     * crits (1.23% fire crit chance with no gear and no talent). The record states the amounts and crit flags.
     *
     * Categories: `staged` the player and the target as staged; `bonus` the spell power part the damage formula
     * adds to every hit, Unit::SpellBonusWithCoeffs over the player's fire spell power, as a number; `casts` each
     * cast's packets: one SPELL_START, one SPELL_GO, then one damage log with its `self.hit.133` line and the
     * target's health lost equal to its amount (or the target in the miss list, no damage log and no health
     * lost), and the mana spent equal to the cost; `amounts` the amounts in cast order as a value (a crit marked
     * `c`, a miss `miss`), each checked to be the bonus plus a base roll in Fireball's band at 85 by the client's
     * rule (Spell_C.cpp), a crit's after taking off the spell crit bonus.
     */
    class MageFireFireballCrit : public SpellScenario
    {
    public:
        MageFireFireballCrit()
            : SpellScenario("mage-fire-fireball-crit", 950,
                            { "template", "staged", "bonus", "casts", "amounts", "noPersistence", "digest" }) {}

        void Prepare() override
        {
            const uint32 FIREBALL = 133;
            const uint32 CAMEL = 50249;
            const uint32 level = 85;

            // Fireball, and the two proc passives every mage is created with (playercreateinfo_spell) with the spells
            // they trigger: Offensive State (79684) casts Arcane Missiles! (79683) on its owner, Deep Freeze Immunity
            // State (71761) names Deep Freeze (71757).
            std::vector<SpellPrint> prints = {
                // 133: 26 fields, fnv 81dba6e7
                { FIREBALL, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            QuestPlan plan;
            plan.quest = 0;
            plan.classId = CLASS_MAGE;
            plan.level = level;
            plan.casts.insert(FIREBALL);
            plan.casts.insert(79683);
            plan.dealsDamage = true;
            std::string templateOk;
            QuestPreCheck pre;
            if (!Qualify(prints, { CAMEL }, plan, templateOk, &pre))
            {
                return;
            }

            Player* p = SpawnPlayer(P0.x, P0.y, Ground(P0.x, P0.y, P0.z), 0.0f, CLASS_MAGE);
            if (!p)
            {
                Verdict(Invalid("spawn failed"));
                return;
            }
            const float tx = P0.x + 10.0f;
            Creature* target = SpawnTarget(CAMEL, level, tx, P0.y, Ground(tx, P0.y, P0.z), 3.14159f);
            if (!target)
            {
                Verdict(Invalid("the target (50249) did not spawn"));
                return;
            }
            StartTimeSync(p);

            auto st = std::make_shared<St>();
            st->player = p->GetObjectGuid();
            st->target = target->GetObjectGuid();
            st->templateOk = templateOk;
            st->noReachLevel = pre.levelBound == pre.startLevel;
            st->achievementsAtSpawn = Achievements(p);
            st->dealsDamage = plan.dealsDamage;
            st->plan = plan;
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
            stage.spellPower = kSpellPower;
            st->staged = StagePlayer(*p, stage);
            st->levelSet = p->getLevel();
            ReadStage(p, target, level, st->staged, st->stagedText, st->stagedOk);
            SpellEntry const* info = sSpellStore.LookupEntry(FIREBALL);
            ReadBonus(p, target, info, st->spellPower, st->bonus, st->bonusText);
            ScaledBand(info, p->getLevel(), st->bandLow, st->bandHigh, st->bandRefusal);
            st->casts.resize(kFireballCasts);

            for (uint32 k = 0; k < kFireballCasts; ++k)
            {
                At(300 + kCastEveryMs * k, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    Creature* target = Get(st->target);
                    SpellEntry const* info = sSpellStore.LookupEntry(FIREBALL);
                    if (!p || !target || !info || !st->staged) { return; }
                    Cast& c = st->casts[k];
                    const std::string name = "cast" + U(k + 1);
                    m_rec.Open(name);
                    const std::string procSelf = ProcHolders(p);
                    const std::string procTarget = ProcHolders(target);
                    m_rec.Note("holders that can proc: self [" + procSelf + "], target [" + procTarget + "]");
                    const std::string unprintedSelf = ProcHolders(p, st->printed);
                    const std::string unprintedTarget = ProcHolders(target, st->printed);
                    const std::string unprinted = unprintedSelf + (unprintedSelf.empty() || unprintedTarget.empty() ? "" : ",") + unprintedTarget;
                    if (!unprinted.empty())
                    {
                        // a proc the guard did not fingerprint could be reached: refused before the cast
                        c.refused = true;
                        m_rec.Note("holders that can proc and are not fingerprinted: [" + unprinted + "]; not cast");
                        m_rec.Open(name + "+tick");
                        return;
                    }
                    p->SetPower(POWER_MANA, p->GetMaxPower(POWER_MANA));
                    c.manaBefore = p->GetPower(POWER_MANA);
                    c.cost = Spell::CalculatePowerCost(info, p);
                    c.healthBefore = target->GetHealth();
                    m_rec.Note("mana " + U(c.manaBefore) + ", cost " + U(c.cost) + "; target health " + U(c.healthBefore) + "/" +
                               U(target->GetMaxHealth()));
                    c.castId = CastAsHandler(p, FIREBALL, target, uint8(k + 1));
                    m_rec.Open(name + "+tick");
                });
                At(300 + kCastEveryMs * k + kReadAfterMs, [this, st, k]()
                {
                    Player* p = sPlayerRegistry.Find(st->player);
                    Creature* target = Get(st->target);
                    Cast& c = st->casts[k];
                    if (!p || !target || !c.castId) { return; }
                    const std::string name = "cast" + U(k + 1) + "-read";
                    m_rec.Open(name);
                    c.read = true;
                    c.healthAfter = target->GetHealth();
                    c.targetAlive = target->IsAlive();
                    m_rec.Note("target health " + U(c.healthAfter) + "/" + U(target->GetMaxHealth()) + "; mana " + U(p->GetPower(POWER_MANA)));
                    m_rec.Open(name + "+tick");
                });
            }

            At(300 + kCastEveryMs * kFireballCasts, [this, st]()
            {
                m_rec.End();   // the sink comes off before the verdict
                Player* p = sPlayerRegistry.Find(st->player);
                if (!p)
                {
                    Verdict(Invalid("the harness player was gone at the verdict"));
                    return;
                }
                if (!st->staged)
                {
                    const std::string why = "INVALID(StagePlayer refused; its ERR line names the step)";
                    Verdict(Compose({ st->templateOk, why, why, why, why,
                                      NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan),
                                      DigestValue("the first cast") }));
                    return;
                }
                const std::string staged = std::string(st->stagedOk ? "OK" : "BUG") + "(" + st->stagedText + ")";
                const std::string bonus = U(uint32(st->bonus)) + "(" + st->bonusText + ")";
                std::string casts, amounts;
                ReadCasts(*st, FIREBALL, casts, amounts);
                Verdict(Compose({ st->templateOk, staged, bonus, casts, amounts,
                                  NoPersistence(p, st->achievementsAtSpawn, st->levelSet, st->noReachLevel, st->dealsDamage, &st->plan),
                                  DigestValue("the first cast") }));
            });
        }

    private:
        static const uint32 kFireballCasts = 5;
        static const uint32 kCastEveryMs = 4100;
        static const uint32 kReadAfterMs = 3600;
        static const int32 kSpellPower = 1000;

        /// One cast, as its two steps saw it.
        struct Cast
        {
            uint32 castId = 0;
            bool refused = false;
            uint32 manaBefore = 0, cost = 0;
            uint32 healthBefore = 0, healthAfter = 0;
            bool read = false, targetAlive = false;
        };
        struct St
        {
            ObjectGuid player;
            ObjectGuid target;
            std::string templateOk;
            uint32 levelSet = 0;
            bool noReachLevel = false;
            std::set<uint32> achievementsAtSpawn;
            bool dealsDamage = false;
            QuestPlan plan;
            bool staged = false;
            std::string stagedText;
            bool stagedOk = false;
            int32 spellPower = 0;
            int32 bonus = 0;
            std::string bonusText;
            int32 bandLow = 0, bandHigh = 0;
            std::string bandRefusal;
            std::set<uint32> printed;
            std::vector<Cast> casts;
        };

        /// The staged player and the target, as the `staged` category prints them; `ok` when the level is the plan's,
        /// the free talent points are the level's own, every glyph slot is open (0x1ff, from level 75), and the fire
        /// spell power is the staged amount plus the intellect's part and nothing else.
        void ReadStage(Player* p, Creature* target, uint32 level, bool staged, std::string& text, bool& ok)
        {
            const SpellSchoolMask fire = SPELL_SCHOOL_MASK_FIRE;
            const int32 power = p->SpellBaseDamageBonusDone(fire);
            const int32 intellect = int32(p->GetStat(STAT_INTELLECT));
            const int32 fromIntellect = std::max(0, intellect - 10);
            const uint32 points = TalentMgr::CalculateTalentsPoints(p->getLevel(), p->getClass(),
                                                                    sGameConfig.getConfig(CONFIG_FLOAT_RATE_TALENT), 0);
            char buf[640];
            snprintf(buf, sizeof(buf),
                     "level %u; free talent points %u of the level's %u, primary tree %u; glyph slots open 0x%x; fire spell power %d = %d staged + intellect %d - 10; spell crit %.2f%% (fire); haste rating %u, mastery rating %u; mana %u/%u; target %u level %u, health %u/%u, fire resistance %u",
                     p->getLevel(), p->GetTalentMgr().FreePoints(), points, p->GetTalentMgr().PrimaryTree(p->GetTalentMgr().ActiveSpec()),
                     p->GetUInt32Value(PLAYER_GLYPHS_ENABLED), power, kSpellPower, intellect,
                     p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE),
                     p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_HASTE_SPELL), p->GetUInt32Value(PLAYER_FIELD_COMBAT_RATING_1 + CR_MASTERY),
                     p->GetPower(POWER_MANA), p->GetMaxPower(POWER_MANA), target->GetEntry(), target->getLevel(),
                     target->GetHealth(), target->GetMaxHealth(), target->GetResistance(SPELL_SCHOOL_FIRE));
            text = std::string(buf) + "; holders that can proc: self [" + ProcHolders(p) + "] (by chance [" + LiveProcHolders(p) + "]), target [" +
                   ProcHolders(target) + "]";
            ok = staged && p->getLevel() == level && p->GetTalentMgr().FreePoints() == points && p->GetUInt32Value(PLAYER_GLYPHS_ENABLED) == 0x1ff &&
                 power == kSpellPower + fromIntellect && target->getLevel() == level;
            m_rec.Note("staged: " + text);
        }

        /// The spell power part the damage formula adds to every hit of `info`: Unit::SpellBonusWithCoeffs over the
        /// player's fire spell power, done side; the taken side's benefit is the target's, read and printed.
        void ReadBonus(Player* p, Creature* target, SpellEntry const* info, int32& power, int32& bonus, std::string& text)
        {
            const SpellSchoolMask school = GetSpellSchoolMask(info);
            power = p->SpellBaseDamageBonusDone(school);
            bonus = p->SpellBonusWithCoeffs(info, 0, power, 0, SPELL_DIRECT_DAMAGE, true, SPELL_BONUS_KIND_DAMAGE);
            SpellBonusEntry const* row = sSpellMgr.GetSpellBonusData(info->ID);
            const float client = EffectBonusCoefficient(info, p->getLevel(), SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE);
            const float applies = SpellPowerCoefficient(client, row, SPELL_BONUS_KIND_DAMAGE, SPELL_DIRECT_DAMAGE,
                                                        info->HasAttribute(SPELL_ATTR_EX3_NO_DONE_BONUS),
                                                        school == SPELL_SCHOOL_MASK_NORMAL);
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "Unit::SpellBonusWithCoeffs over fire spell power %d: Fireball's EffectBonusCoefficient at level %u %.4f, the coefficient that applies %.4f; the target's taken benefit %d",
                     power, p->getLevel(), client, applies, target->SpellBaseDamageBonusTaken(school));
            text = buf;
            m_rec.Note("bonus " + U(uint32(bonus)) + ": " + text);
        }

        /// Fireball's base points at `level` by the client's rule (Spell_C.cpp: the scaling coefficient times the
        /// gtSpellScaling value, its integer, then half the variance of that either side). Above the scaling row's
        /// cast-time and nerf levels only, where the client's scaling multiplier is 1; `refusal` says why otherwise.
        void ScaledBand(SpellEntry const* info, uint32 level, int32& low, int32& high, std::string& refusal)
        {
            SpellScalingEntry const* scaling = info->GetSpellScaling();
            if (!scaling || !scaling->Coefficient[0])
            {
                refusal = "no SpellScaling row with an effect 0 coefficient";
                return;
            }
            if (level < scaling->CastTimeMaxLevel || level < scaling->NerfMaxLevel)
            {
                refusal = "level " + U(level) + " is below the scaling row's levels";
                return;
            }
            const uint32 row = scaling->Class == uint32(-1) ? uint32(MAX_CLASSES - 1) : scaling->Class - 1;
            GtSpellScalingEntry const* gt = sGtSpellScalingStore.LookupEntry(row * GT_MAX_LEVEL + level - 1);
            if (!gt)
            {
                refusal = "no gtSpellScaling value for class " + U(scaling->Class) + " at level " + U(level);
                return;
            }
            const int32 points = int32(scaling->Coefficient[0] * gt->value);
            const int32 half = int32(float(points) * scaling->Variance[0]) / 2;
            low = points - half;
            high = points + half;
        }

        /// The `casts` and `amounts` categories, from the recorded packets of each cast's windows.
        void ReadCasts(St const& st, uint32 spellId, std::string& casts, std::string& amounts) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            SpellEntry const* info = sSpellStore.LookupEntry(spellId);
            const uint32 dmgClass = info ? info->GetDmgClass() : 0;
            std::string castBug, castText, values, fitBug, rolls, castTime;
            for (uint32 k = 0; k < st.casts.size(); ++k)
            {
                Cast const& c = st.casts[k];
                const std::string name = "cast" + U(k + 1);
                const std::string sep = k ? "; " : "";
                values += k ? "," : "";
                if (c.refused)
                {
                    values += "refused";
                    castBug = castBug.empty() ? name + ": refused before the cast, a holder the guard did not fingerprint can proc" : castBug;
                    continue;
                }

                std::vector<Recorder::Seen const*> starts = m_rec.SeenIn(name, SMSG_SPELL_START);
                std::string startRecord;
                if (starts.size() == 1 && !starts[0]->payload.empty())
                {
                    Trace::DecodeSpellCast(false, &starts[0]->payload[0], starts[0]->payload.size(), roles, startRecord);
                }
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(name + "+tick", SMSG_SPELL_GO);
                std::string goRecord;
                if (gos.size() == 1 && !gos[0]->payload.empty())
                {
                    Trace::DecodeSpellCast(true, &gos[0]->payload[0], gos[0]->payload.size(), roles, goRecord);
                }
                const bool hit = goRecord.find(" hit=[target] miss=[]") != std::string::npos;
                const bool missed = goRecord.find(" hit=[] miss=[target:") != std::string::npos;
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(name + "+tick", SMSG_SPELLNONMELEEDAMAGELOG);
                std::vector<Recorder::Seen const*> missLogs = m_rec.SeenIn(name + "+tick", SMSG_SPELLLOGMISS);
                const uint32 failed = m_rec.CountIn(name, SMSG_CAST_FAILED) + m_rec.CountIn(name + "+tick", SMSG_CAST_FAILED);

                // the first power update of the player's mana after the cast: the cost taken
                uint32 firstMana = 0;
                bool manaRead = false;
                std::vector<Recorder::Seen const*> powers = m_rec.SeenIn(name + "+tick", SMSG_POWER_UPDATE);
                for (size_t i = 0; i < powers.size() && !manaRead; ++i)
                {
                    uint64 unit = 0;
                    uint8 type = 0;
                    uint32 value = 0;
                    if (!powers[i]->payload.empty() &&
                        Trace::ReadPowerUpdate(&powers[i]->payload[0], powers[i]->payload.size(), unit, type, value) &&
                        unit == roles.self && type == POWER_MANA)
                    {
                        firstMana = value;
                        manaRead = true;
                    }
                }

                // the damage log, as its `hit` line reads it
                uint32 logSpell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                bool logRead = false;
                std::string hitLine;
                uint64 logTarget = 0, logAttacker = 0;
                if (logs.size() == 1 && !logs[0]->payload.empty())
                {
                    std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &logs[0]->payload[0],
                                                                             logs[0]->payload.size(), roles);
                    uint32 ignoredSpell = 0, ignoredDamage = 0, overkill = 0;
                    hitLine = fields.size() == 1 ? fields[0] : std::string();
                    logRead = fields.size() == 1 &&
                              sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &logSpell, &amount, &crit, &absorbed, &resisted) == 5 &&
                              Trace::ReadSpellDamage(&logs[0]->payload[0], logs[0]->payload.size(), logTarget, logAttacker, ignoredSpell,
                                                     ignoredDamage, overkill);
                }

                char problem[512] = "";
                if (!c.castId)
                {
                    snprintf(problem, sizeof(problem), "the spellbook refused %u", spellId);
                }
                else if (!c.read)
                {
                    snprintf(problem, sizeof(problem), "the read step did not run");
                }
                else if (starts.size() != 1 || startRecord.find("spell=" + U(spellId) + " flags=") != 0 ||
                         startRecord.find(" source=self caster=self ") == std::string::npos || failed)
                {
                    snprintf(problem, sizeof(problem), "SPELL_START x%u [%s], CAST_FAILED x%u", uint32(starts.size()), startRecord.c_str(), failed);
                }
                else if (gos.size() != 1 || (!hit && !missed))
                {
                    snprintf(problem, sizeof(problem), "SPELL_GO x%u [%s]", uint32(gos.size()), goRecord.c_str());
                }
                else if (hit && (!logRead || !missLogs.empty() || logSpell != spellId || logTarget != roles.target ||
                                 amount != c.healthBefore - c.healthAfter || !c.targetAlive))
                {
                    snprintf(problem, sizeof(problem), "damage logs x%u [%s], miss logs x%u; target health %u -> %u, alive %d",
                             uint32(logs.size()), hitLine.c_str(), uint32(missLogs.size()), c.healthBefore, c.healthAfter, c.targetAlive ? 1 : 0);
                }
                else if (missed && (!logs.empty() || c.healthBefore != c.healthAfter))
                {
                    snprintf(problem, sizeof(problem), "a miss in SPELL_GO, then damage logs x%u; target health %u -> %u",
                             uint32(logs.size()), c.healthBefore, c.healthAfter);
                }
                else if (!manaRead || firstMana != c.manaBefore - c.cost)
                {
                    snprintf(problem, sizeof(problem), "mana %u, cost %u, the first power update %u", c.manaBefore, c.cost, firstMana);
                }
                if (problem[0] && castBug.empty())
                {
                    castBug = name + ": " + problem;
                }
                if (castTime.empty())
                {
                    const size_t at = startRecord.find(" casttime=");
                    castTime = at == std::string::npos ? std::string("?") : startRecord.substr(at + 10, startRecord.find(' ', at + 10) - at - 10);
                }

                if (!hit || !logRead)
                {
                    values += missed ? "miss" : "none";
                    castText += sep + U(k + 1) + " " + (missed ? "missed (in SPELL_GO's miss list, miss logs x" + U(missLogs.size()) + ")" : std::string("unread"));
                    continue;
                }
                values += U(amount) + (crit ? "c" : "");
                castText += sep + U(k + 1) + " " + hitLine.substr(4) + ", health " + U(c.healthBefore) + "->" + U(c.healthAfter) +
                            ", mana " + U(c.manaBefore) + "->" + U(firstMana);

                // what the damage was before the spell crit bonus, then the base roll under the bonus
                const int32 dealt = int32(amount + absorbed + resisted);
                int32 beforeCrit = crit ? -1 : dealt;
                for (int32 d = (2 * dealt) / 3 - 1; crit && d <= (2 * dealt) / 3 + 1; ++d)
                {
                    if (d > 0 && d + Combat::SpellCritDamageBonusBase(uint32(d), dmgClass, 0) == dealt)
                    {
                        beforeCrit = d;
                    }
                }
                const int32 roll = beforeCrit - st.bonus;
                rolls += (rolls.empty() ? "" : ",") + std::to_string(roll);
                if (fitBug.empty() && (beforeCrit < 0 || roll < st.bandLow || roll > st.bandHigh))
                {
                    fitBug = name + ": " + U(amount) + (crit ? " (crit)" : "") + " is no base roll in " + std::to_string(st.bandLow) + ".." +
                             std::to_string(st.bandHigh) + " plus the bonus " + std::to_string(st.bonus);
                }
            }

            casts = castBug.empty()
                    ? "OK(" + U(st.casts.size()) + " casts, each one SPELL_START (casttime=" + castTime +
                      "), one SPELL_GO and its log; the target lost each amount; mana by the cost, " + U(st.casts.empty() ? 0 : st.casts[0].cost) +
                      ": " + castText + ")"
                    : "BUG(" + castBug + ")";
            if (values.empty())
            {
                values = "none";
            }
            if (!st.bandRefusal.empty())
            {
                amounts = "INVALID(" + values + ": " + st.bandRefusal + ")";
            }
            else if (!fitBug.empty())
            {
                amounts = "BUG(" + values + ": " + fitBug + ")";
            }
            else
            {
                amounts = values + "(each the bonus " + std::to_string(st.bonus) + " plus a base roll in Fireball's band " +
                          std::to_string(st.bandLow) + ".." + std::to_string(st.bandHigh) + " at 85, a crit's taken back off the 50% spell crit bonus first; the rolls " +
                          rolls + ")";
            }
        }
    };

    /**
     * S951 `mage-fire-hot-streak`: Hot Streak (44445) and Improved Hot Streak (44448, rank 2 of 2) on a staged Fire
     * mage. Hot Streak's dummy aura procs on each Fireball hit: a crit rolls the base chance, which falls with his sheet
     * spell crit and is 0 at the crit aura's +100, then counts, and the second crit in a row rolls Improved Hot Streak's
     * 100% for Hot Streak (48108); a hit that does not crit starts the count again. Pyroblast! (92315), which 48108
     * makes castable, neither counts for nor breaks the streak, and casting it uses 48108 up. Staged: Burning Soul 3,
     * Improved Fire Blast 2, Fire Power 3, Blazing Speed 2, Hot Streak, Improved Scorch 2 and Cauterize 2 (the fifteen
     * points the fourth tier asks), then Improved Hot Streak 2. Ten casts: Fireball crit, Fireball crit, Pyroblast!
     * crit, Fireball hit, Fireball crit, Fireball hit, then four Fireball crits; a cast that must not crit takes the crit
     * aura at -100. The second crits in a row are casts 2, 8 and 10, the cast of 10 while the 48108 of 8 is still up.
     * One cast every 4.3 s.
     *
     * `streak`: the casts after which 48108 was cast, then `gone=` the casts at whose read 48108 was gone from him while
     * he held it when the cast ran; OK when the first are exactly the second crits in a row of the casts other than
     * Pyroblast!, and the second exactly the Pyroblast! casts.
     */
    class MageFireHotStreak : public MageProcScenario
    {
    public:
        MageFireHotStreak() : MageProcScenario("mage-fire-hot-streak", 951, "streak") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 1 }, { 11433, 1 }, { 12121, 1 } };
        }

        std::vector<Step> Steps() const override
        {
            return { Cast(133, true), Cast(133, true), Cast(kPyroblastInstant, true), Cast(133, false), Cast(133, true),
                     Cast(133, false), Cast(133, true), Cast(133, true), Cast(133, true), Cast(133, true) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 92315: 49 fields, fnv 7ef90b4a
                { 92315, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44448: 49 fields, fnv 2ada6a76
                { 44448, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11367: 26 fields, fnv 5d9994de
                { 11367, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86949: 49 fields, fnv 7082c5a5
                { 86949, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 40 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 48108; }
        uint32 CastEveryMs() const override { return 4300; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::string Own(St const& st) const override
        {
            std::vector<bool> second(st.steps.size(), false);
            uint32 count = 0;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                if (st.steps[k].spell == kPyroblastInstant)
                {
                    continue;
                }
                Seen const& seen = st.seen[k];
                count = seen.logRead && seen.crit ? count + 1 : 0;
                if (count == 2)
                {
                    second[k] = true;
                    count = 0;
                }
            }
            const std::string casts = StepsWhere(st, [&st](uint32 k) { return Triggered(st.seen[k], 48108); });
            const std::string reached = StepsWhere(st, [&second](uint32 k) { return second[k]; });
            const std::string gone = StepsWhere(st, [&st](uint32 k)
            {
                return !st.seen[k].watchedBefore.empty() && st.seen[k].watchedBefore != "none" && st.seen[k].holder == "none";
            });
            const std::string pyroblasts = StepsWhere(st, [&st](uint32 k) { return st.steps[k].spell == kPyroblastInstant; });
            const bool ok = casts != "none" && casts == reached && gone == pyroblasts;
            const std::string value = casts + " gone=" + gone;
            const std::string detail = "48108 cast after casts " + casts + "; the second crits in a row of the casts other than Pyroblast! at casts " +
                                       reached + "; 48108 on him when the cast ran and gone at its read at casts " + gone +
                                       "; Pyroblast! (92315) at casts " + pyroblasts + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kPyroblastInstant = 92315;
    };

    /**
     * S952 `mage-fire-master-of-elements`: Master of Elements (29075, rank 2 of 2) on a staged Fire mage. Its dummy
     * aura procs on the crits of his Fire and Frost spells and gives back the rank's 30% of the spell's mana cost
     * through 29077. Staged: Master of Elements 2, and Frostbolt (116) learned. Casts: Fireball crit, Fireball hit,
     * Frostbolt crit, Frostbolt hit.
     *
     * `refund`: the amount 29077 gave back after each cast, `-` when nothing; OK when each crit gave back 30% of its
     * cast's cost and no hit gave anything.
     */
    class MageFireMasterOfElements : public MageProcScenario
    {
    public:
        MageFireMasterOfElements() : MageProcScenario("mage-fire-master-of-elements", 952, "refund") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 10545, 1 } }; }
        std::vector<uint32> Spells() const override { return { 116 }; }

        std::vector<Step> Steps() const override
        {
            return { Cast(133, true), Cast(133, false), Cast(116, true), Cast(116, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 116: 72 fields, fnv e7ba11a7
                { 116, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 17 }, { "eff1.EffectBonusCoefficient", 1064396915 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 3 }, { "eff1.EffectRealPointsPerLevel", 1056964608 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 118 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            std::string value, text;
            bool ok = true;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::string amounts = EnergizeAmounts(k, 29077);
                value += (k ? "," : "") + (amounts.empty() ? std::string("-") : amounts);
                text += (k ? "; " : "") + U(k + 1) + " " + U(st.steps[k].spell) + (st.seen[k].crit ? " crit" : " hit") + ", cost " +
                        U(st.seen[k].cost) + ": " + (amounts.empty() ? std::string("none") : amounts);
                const std::string wanted = st.seen[k].crit ? U(st.seen[k].cost * 30 / 100) : std::string();
                ok = ok && amounts == wanted;
            }
            return ok ? value + "(OK: " + text + ")" : "BUG(" + value + ": " + text + ")";
        }
    };

    /**
     * S954 `mage-fire-blazing-speed`: Blazing Speed (31642, rank 2 of 2) on a staged Fire mage. Its proc-trigger aura
     * procs on the melee and ranged hits he takes, at its 10%, and casts Blazing Speed (31643) on him; the trigger spell
     * its effect names (18350) is absent from the client's data. Staged: Burning Soul 3 and Improved Fire Blast 2 (the
     * five points the second tier asks), then Blazing Speed 2. The Camel swings at him kSwings times.
     *
     * `blazingSpeed`: the swings after which 31643 was cast; OK when each of them hit him and at least one did.
     */
    class MageFireBlazingSpeed : public MageProcScenario
    {
    public:
        MageFireBlazingSpeed() : MageProcScenario("mage-fire-blazing-speed", 954, "blazingSpeed") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 10531, 2 }, { 10523, 1 }, { 10555, 1 } }; }

        std::vector<Step> Steps() const override
        {
            return std::vector<Step>(kSwings, Swing());
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 31643: 72 fields, fnv b499aeb8
                { 31643, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 108 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 11 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 31643; }

        std::string Own(St const& st) const override
        {
            const std::string casts = StepsWhere(st, [&st](uint32 k) { return Triggered(st.seen[k], 31643); });
            const std::string hits = StepsWhere(st, [&st](uint32 k) { return st.seen[k].healthAfter < st.seen[k].healthBefore; });
            bool ok = casts != "none";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                ok = ok && (!Triggered(st.seen[k], 31643) || st.seen[k].healthAfter < st.seen[k].healthBefore);
            }
            const std::string detail = "31643 cast after swings " + casts + "; the swings that hit: " + hits + ")";
            return ok ? casts + "(OK: " + detail : "BUG(" + casts + ": " + detail;
        }

    private:
        static constexpr uint32 kSwings = 30;
    };

    /**
     * S955 `mage-fire-combustion`: Combustion (11129) on a staged Fire mage puts its DoT (83853) on the Camel with a
     * tick of the per-second damage of his DoTs there: here his Pyroblast's DoT (11366, every 3 s) and the Ignite
     * (12654, every 2 s) of the Pyroblast's crit. Staged: Burning Soul 3, Improved Fire Blast 2, Ignite 3, Fire Power 3,
     * Blazing Speed 2, Hot Streak and Improved Scorch 1 (the fifteen points the fourth tier asks; Impact is left out),
     * then Combustion, with kSpellPower spell power as 950 stages it. He learns no Mastery passive, so his mastery is 0
     * and Flashburn adds nothing, and stages no haste, so every period is the client data's. Before each cast he takes
     * Precision (13843, +6% spell hit), so no cast misses a level-85 Camel. Steps: Pyroblast, which must crit (its
     * 3.5 s cast and its flight land it about 3.9 s after the step, before the next), then Combustion kCastEveryMs
     * later, which must not crit (the crit aura on him at NonCritAuraPoints() from then on, so no tick after it crits),
     * read kLastStepMs on, past the DoT's 10 s.
     *
     * `combustion`: `ticks=<n>,each=<x>,ignite=<i>,pyro=<p>`: the 83853 ticks in every window, the first of them, and
     * the first 12654 and 11366 ticks. OK when the Pyroblast crit, Ignite and Pyroblast ticked, 83853 ticked its
     * duration over its period (10) times, every tick x = int(i x 1000 / 2000 + p x 1000 / 3000) with the periods the
     * client data's, and none a crit.
     */
    class MageFireCombustion : public MageProcScenario
    {
    public:
        MageFireCombustion() : MageProcScenario("mage-fire-combustion", 955, "combustion") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 0 }, { 10561, 0 } };
        }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step combustion = Cast(11129, false);
            combustion.everyMs = kLastStepMs;
            return { Cast(11366, true), combustion };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kCombustionDot; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::string Own(St const& st) const override
        {
            // every periodic damage tick of the scenario's windows, by spell, in the order they came
            Trace::Roles const& roles = m_rec.Roles();
            std::map<uint32, std::vector<std::pair<uint32, bool> > > ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5)
                            {
                                ticks[spell].push_back(std::make_pair(amount, crit != 0));
                            }
                        }
                    }
                }
            }

            // the periods and the tick count as the client's data gives them, the mage staging no haste
            const uint32 ignitePeriod = PeriodOf(kIgniteDot, EFFECT_INDEX_0);
            const uint32 pyroblastPeriod = PeriodOf(kPyroblast, EFFECT_INDEX_1);
            const uint32 combustionPeriod = PeriodOf(kCombustionDot, EFFECT_INDEX_0);
            const int32 combustionDuration = GetSpellDuration(sSpellStore.LookupEntry(kCombustionDot));
            const uint32 combustionTicks = combustionPeriod && combustionDuration > 0 ? uint32(combustionDuration) / combustionPeriod : 0;

            std::vector<std::pair<uint32, bool> > const& ignite = ticks[kIgniteDot];
            std::vector<std::pair<uint32, bool> > const& pyroblast = ticks[kPyroblast];
            std::vector<std::pair<uint32, bool> > const& combustion = ticks[kCombustionDot];
            const uint32 i = ignite.empty() ? 0 : ignite[0].first;
            const uint32 p = pyroblast.empty() ? 0 : pyroblast[0].first;
            const uint64 periods = uint64(ignitePeriod) * pyroblastPeriod;
            const uint32 expected = periods ? uint32((uint64(i) * 1000 * pyroblastPeriod + uint64(p) * 1000 * ignitePeriod) / periods) : 0;

            bool ok = st.steps.size() == 2 && st.seen[0].logRead && st.seen[0].crit && !ignite.empty() && !pyroblast.empty() &&
                      combustionTicks == 10 && combustion.size() == combustionTicks;
            std::string combustionText;
            for (size_t t = 0; t < combustion.size(); ++t)
            {
                ok = ok && combustion[t].first == expected && !combustion[t].second;
                combustionText += (t ? " " : "") + U(combustion[t].first) + (combustion[t].second ? "c" : "");
            }
            std::string igniteText;
            for (size_t t = 0; t < ignite.size(); ++t)
            {
                igniteText += (t ? " " : "") + U(ignite[t].first) + (ignite[t].second ? "c" : "");
            }
            std::string pyroblastText;
            for (size_t t = 0; t < pyroblast.size(); ++t)
            {
                pyroblastText += (t ? " " : "") + U(pyroblast[t].first) + (pyroblast[t].second ? "c" : "");
            }

            const std::string value = "ticks=" + U(combustion.size()) + ",each=" +
                                      (combustion.empty() ? std::string("none") : U(combustion[0].first)) + ",ignite=" + U(i) +
                                      ",pyro=" + U(p);
            Seen const& pyroblastStep = st.seen[0];
            Seen const& combustionStep = st.seen.size() > 1 ? st.seen[1] : st.seen[0];
            const std::string read = combustionStep.periodicRead
                                     ? "period " + std::to_string(combustionStep.period) + ", duration " +
                                       std::to_string(combustionStep.duration) + "/" + std::to_string(combustionStep.maxDuration)
                                     : std::string("no periodic effect");
            Player* mage = sPlayerRegistry.Find(st.player);
            const int32 spellPower = mage ? mage->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE) : 0;
            char buf[768];
            snprintf(buf, sizeof(buf),
                     "Pyroblast %u%s; 12654 ticks [%s]; 11366 ticks [%s]; 83853 ticks [%s], %u wanted (%d ms over a period of %u), "
                     "each int(%u x 1000 / %u + %u x 1000 / %u) = %u; Combustion's direct hit %u%s; 83853 on the Camel at step 2's read: %s; "
                     "fire spell power %d",
                     pyroblastStep.amount, pyroblastStep.crit ? " crit" : "", igniteText.c_str(), pyroblastText.c_str(),
                     combustionText.c_str(), combustionTicks, combustionDuration, combustionPeriod, i, ignitePeriod, p, pyroblastPeriod,
                     expected, combustionStep.amount, combustionStep.crit ? " crit" : "", read.c_str(), spellPower);
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCastEveryMs = 4300;
        static constexpr uint32 kLastStepMs = 12000;

        /// The period of `spell`'s effect `effIndex` in the client's data; 0 when the store has no such effect.
        static uint32 PeriodOf(uint32 spell, SpellEffectIndex effIndex)
        {
            SpellEntry const* entry = sSpellStore.LookupEntry(spell);
            SpellEffectEntry const* effect = entry ? entry->GetSpellEffect(effIndex) : NULL;
            return effect ? effect->EffectAuraPeriod : 0;
        }
    };

    /**
     * S956 `mage-fire-living-bomb`: Living Bomb (44457) on the Camel, cast by a staged mage who learned the talent's
     * spell (which takes its one point), with Glyph of Living Bomb (928) and kHasteRating haste rating, as a level-85
     * Fire mage has them. Its dummy aura (effect 1) holds the explosion's spell, 44461, which the Camel casts for the
     * mage when the aura runs out, and not when it is removed otherwise. Steps: Living Bomb, left to run out its 12 s;
     * Living Bomb, then dispelled; Living Bomb, then removed by the plain call.
     *
     * `explosion`: the steps after which 44461 was cast, with its damage logs; OK when they are the expiry's (1) and
     * no other.
     */
    class MageFireLivingBomb : public MageProcScenario
    {
    public:
        MageFireLivingBomb() : MageProcScenario("mage-fire-living-bomb", 956, "explosion") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { 44457 }; }
        std::vector<uint32> Glyphs() const override { return { 928 }; }
        int32 HasteRating() const override { return kHasteRating; }
        bool TakesDamage() const override { return true; }

        std::vector<Step> Steps() const override
        {
            return { AuraCast(44457, kExpiryEveryMs), AuraCast(44457, 0), Removal(44457, AURA_REMOVE_BY_DISPEL, 0), AuraCast(44457, 0),
                     Removal(44457, AURA_REMOVE_BY_DEFAULT, 0) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 44457; }
        bool WatchedOnTarget() const override { return true; }
        bool NotesBeforeStep() const override { return true; }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::string logs;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            if (fields[f].find(".hit.44461=") != std::string::npos)
                            {
                                logs += (logs.empty() ? "" : "; ") + U(k + 1) + " " + fields[f];
                            }
                        }
                    }
                }
            }
            const std::string casts = StepsWhere(st, [&st](uint32 k) { return Triggered(st.seen[k], 44461); });
            const std::string detail = "44461 cast after steps " + casts + "; its damage logs: " + (logs.empty() ? std::string("none") : logs) + ")";
            return casts == "1" ? casts + "(OK: " + detail : "BUG(" + casts + ": " + detail;
        }

    private:
        static constexpr int32 kHasteRating = 1500;
        static constexpr uint32 kExpiryEveryMs = 14000;
    };

    /**
     * S965 `mage-fire-living-bomb-limit`: Living Bomb (44457) on four Camels, cast by a staged mage as 956's (the spell
     * learned, Glyph of Living Bomb 928) but without haste rating, so a bomb lasts its 12 s. Its text ends "Limit 3
     * targets.": a caster keeps his Living Bomb on three targets at most, and a fourth takes it off the oldest, which
     * does not explode. The Camels stand kTargetSpacing apart, beyond the explosion's 10 yards from one another. Before
     * the first cast he takes Precision (13843, +6% spell hit), so no cast can miss a level-85 Camel. Steps, one Living
     * Bomb each, read kReadMs after its cast: at Camels 1, 2, 3 and 4, 2 s apart, the fourth 3 s before the next; then
     * at Camel 2 again, 9 s after the first, between that bomb's ticks at 8 and 11 s, and the 16 s after it cover every
     * bomb's end.
     *
     * `limit`: `limited=<each Camel a cast took his bomb off, with its remove mode>,alive=<how many Camels hold a bomb
     * at the last read>,explosions=<the casts of 44461>`; OK when the fourth cast took Camel 1's bomb with
     * AURA_REMOVE_BY_TARGET_LIMIT and no cast took another, Camels 2, 3 and 4 hold one at the last read, and each of
     * them, never Camel 1, cast 44461 once and took its one damage log.
     */
    class MageFireLivingBombLimit : public MageProcScenario
    {
    public:
        MageFireLivingBombLimit() : MageProcScenario("mage-fire-living-bomb-limit", 965, "limit") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { kLivingBomb }; }
        std::vector<uint32> Glyphs() const override { return { 928 }; }
        bool TakesDamage() const override { return true; }
        uint32 TargetCount() const override { return 4; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 CasterAura() const override { return kPrecision; }

        std::vector<Step> Steps() const override
        {
            return { AuraCastAt(kLivingBomb, 0, 2000), AuraCastAt(kLivingBomb, 1, 2000), AuraCastAt(kLivingBomb, 2, 2000),
                     AuraCastAt(kLivingBomb, 3, 3000), AuraCastAt(kLivingBomb, 1, 16000) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kLivingBomb; }
        bool WatchedOnTarget() const override { return true; }

        std::string Own(St const& st) const override
        {
            std::string limited, limitedText;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                if (!st.seen[k].removed.empty())
                {
                    limited += (limited.empty() ? "" : ";") + st.seen[k].removed;
                    limitedText += (limitedText.empty() ? "" : "; ") + std::string("step ") + U(k + 1) + " " + st.seen[k].removed;
                }
            }

            Seen const& last = st.seen.back();
            std::string alive;
            uint32 aliveCount = 0;
            for (uint32 camel = 0; camel < last.holders.size(); ++camel)
            {
                if (last.holders[camel] != "none")
                {
                    ++aliveCount;
                    alive += (alive.empty() ? "" : ",") + U(camel + 1);
                }
            }

            // each SPELL_GO of the explosion and each damage log of it, by the Camel its decode names `target`
            std::vector<uint32> casts(st.targets.size(), 0), logs(st.targets.size(), 0);
            const std::string spellField = " spell=" + U(kExplosion) + " ";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = "step" + U(k + 1) + suffix;
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    std::vector<Recorder::Seen const*> damage = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (uint32 camel = 0; camel < st.targets.size(); ++camel)
                    {
                        const Trace::Roles roles = RolesAt(st, camel);
                        for (size_t i = 0; i < gos.size(); ++i)
                        {
                            std::string go;
                            uint32 spell = 0;
                            if (!gos[i]->payload.empty() && Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) &&
                                sscanf(go.c_str(), "spell=%u", &spell) == 1 && spell == kExplosion && go.find(" caster=target ") != std::string::npos)
                            {
                                ++casts[camel];
                            }
                        }
                        for (size_t i = 0; i < damage.size(); ++i)
                        {
                            std::string log;
                            if (!damage[i]->payload.empty() &&
                                Trace::DecodeSpellDamageLog(&damage[i]->payload[0], damage[i]->payload.size(), roles, log) &&
                                log.compare(0, 14, "target=target ") == 0 && log.find(spellField) != std::string::npos)
                            {
                                ++logs[camel];
                            }
                        }
                    }
                }
            }

            uint32 explosions = 0;
            std::string castText, logText;
            bool ok = alive == "2,3,4";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                ok = ok && st.seen[k].removed == (k == kFourthStep ? std::string("1:targetLimit") : std::string());
            }
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                explosions += casts[camel];
                castText += (camel ? "," : "") + U(casts[camel]);
                logText += (camel ? "," : "") + U(logs[camel]);
                const uint32 wanted = camel ? 1 : 0;
                ok = ok && casts[camel] == wanted && logs[camel] == wanted;
            }
            const std::string value = "limited=" + (limited.empty() ? std::string("none") : limited) + ",alive=" + U(aliveCount) +
                                      ",explosions=" + U(explosions);
            const std::string detail = "the casts took his bomb off: " + (limitedText.empty() ? std::string("none") : limitedText) +
                                       "; at the last read a bomb on Camels " + (alive.empty() ? std::string("none") : alive) + "; " +
                                       U(kExplosion) + " cast at Camels 1-4 " + castText + " times, its damage logs " + logText + ")";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kExplosion = 44461;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kFourthStep = 3;
    };

    /**
     * What the Pyroblast! scenarios share (962, 963): a staged mage who learned Pyroblast! (92315) casts it at the Camel.
     * Its effect 1 is a periodic damage aura of period 3000 over the spell's 12000 ms, and the spell carries
     * AttributesEx5 0x2000, under which the client hastes the period and rounds the tick count. Pyroblast! asks for Hot
     * Streak (48108) on its caster (SpellAuraRestrictions), so each cast step puts it on him first (CasterAura). The
     * Camel's 92315 holder is the watched one, and each read records its periodic timing.
     */
    class MagePyroblastScenario : public MageProcScenario
    {
    public:
        MagePyroblastScenario(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kPyroblast = 92315;
        static constexpr uint32 kHotStreak = 48108;

        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { kPyroblast }; }
        uint32 CasterAura() const override { return kHotStreak; }
        uint32 Watched() const override { return kPyroblast; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 92315: 49 fields, fnv 7ef90b4a
                { 92315, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// The ticks of Pyroblast! in the periodic logs he is sent in step k's four windows, as the recorder's `tick`
        /// lines read them.
        uint32 Ticks(uint32 k) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            const std::string tick = "self.tick." + U(kPyroblast) + "=";
            uint32 ticks = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (const char* suffix : suffixes)
            {
                std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
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
                        if (fields[f].find(tick) != std::string::npos)
                        {
                            ++ticks;
                        }
                    }
                }
            }
            return ticks;
        }

        /// The watched holder's periodic timing a read recorded, as text.
        static std::string PeriodicText(Seen const& seen)
        {
            return seen.periodicRead ? "period " + std::to_string(seen.period) + ", next tick in " + std::to_string(seen.timer) +
                                           ", duration " + std::to_string(seen.duration) + "/" + std::to_string(seen.maxDuration)
                                     : std::string("no holder with a periodic effect");
        }
    };

    /**
     * S962 `mage-fire-pyroblast-haste`: Pyroblast! (92315) at the Camel by a staged mage with kHasteRating haste rating:
     * 3201 / 128.057 (gtCombatRatings at 85) = 24.9966% spell haste, so his cast-haste multiplier is
     * 100 / 124.9966 = 0.80002. The client's rule for a spell with AttributesEx5 0x2000: the period is
     * round(3000 x 0.80002) = 2400, and the duration round(12000 / 2400) = 5 periods, 12000 ms. One cast, read
     * kCastReadMs after it; the kEveryMs before the end cover every tick.
     *
     * `periodic`: `period=<the period of the Camel's 92315 holder's periodic aura, at the read>,duration=<the holder's
     * maximum duration>,ticks=<the ticks of 92315 in the step's windows>`; OK when they are the client's 2400, 12000
     * and 5.
     */
    class MageFirePyroblastHaste : public MagePyroblastScenario
    {
    public:
        MageFirePyroblastHaste() : MagePyroblastScenario("mage-fire-pyroblast-haste", 962, "periodic") {}

    protected:
        int32 HasteRating() const override { return kHasteRating; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, false);
            pyroblast.everyMs = kEveryMs;
            return { pyroblast };
        }

        std::string Own(St const& st) const override
        {
            Seen const& seen = st.seen[0];
            const uint32 ticks = Ticks(0);
            const std::string value = "period=" + std::to_string(seen.period) + ",duration=" + std::to_string(seen.maxDuration) +
                                      ",ticks=" + U(ticks);
            const bool ok = seen.periodicRead && seen.period == kPeriod && seen.maxDuration == kDuration && ticks == kTicks;
            const std::string detail = "the Camel's 92315 holder at the read: " + PeriodicText(seen) + "; " + U(ticks) +
                                       " ticks in the step's windows; the client's rule at the staged multiplier gives period " +
                                       std::to_string(kPeriod) + ", duration " + std::to_string(kDuration) + ", " + U(kTicks) + " ticks)";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr int32 kHasteRating = 3201;
        static constexpr uint32 kEveryMs = 16000;
        static constexpr int32 kPeriod = 2400;
        static constexpr int32 kDuration = 12000;
        static constexpr uint32 kTicks = 5;
    };

    /**
     * S963 `mage-fire-pyroblast-refresh`: Pyroblast! (92315) cast at the Camel twice by a staged mage without haste, the
     * second kRefreshMs after the first. The missile takes the same time on both casts, so the first holder's ticks come
     * 3.0, 6.0 and 9.0 s after the first landing and the second landing 7.8 s after it, 1.2 s before the third tick.
     * The rule for a periodic effect its caster applies again: the time to the next tick is kept and the full duration
     * comes after it, so the new holder lasts 12000 + 1200 = 13200 ms and ticks 9.0, 12.0, 15.0, 18.0 and 21.0 s after
     * the first landing: five ticks after the two before.
     *
     * `refresh`: `duration=<the maximum duration of the Camel's 92315 holder at the second read>,ticks=<the ticks of 92315
     * in both steps' windows>`; OK when they are 13200 and 7.
     */
    class MageFirePyroblastRefresh : public MagePyroblastScenario
    {
    public:
        MageFirePyroblastRefresh() : MagePyroblastScenario("mage-fire-pyroblast-refresh", 963, "refresh") {}

    protected:
        std::vector<Step> Steps() const override
        {
            Step first = Cast(kPyroblast, false);
            first.everyMs = kRefreshMs;
            Step second = Cast(kPyroblast, false);
            second.everyMs = kEveryMs;
            return { first, second };
        }

        std::string Own(St const& st) const override
        {
            Seen const& second = st.seen[1];
            const uint32 before = Ticks(0), after = Ticks(1);
            const std::string value = "duration=" + std::to_string(second.maxDuration) + ",ticks=" + U(before + after);
            const bool ok = second.periodicRead && second.maxDuration == kDuration && before + after == kTicks;
            const std::string detail = "the Camel's 92315 holder at the first read: " + PeriodicText(st.seen[0]) + "; at the second read: " +
                                       PeriodicText(second) + "; ticks " + U(before) + " in the first step's windows and " + U(after) +
                                       " in the second's; the rule gives duration " + std::to_string(kDuration) + " and " + U(kTicks) +
                                       " ticks)";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kRefreshMs = 7800;
        static constexpr uint32 kEveryMs = 15000;
        static constexpr int32 kDuration = 13200;
        static constexpr uint32 kTicks = 7;
    };

    /**
     * S964 `mage-fire-ignite-mastery`: Ignite (12846, rank 3 of 3) under Flashburn (76595) on a staged Fire mage.
     * Staged: Burning Soul 3 and Improved Fire Blast 2 (the five points the second tier asks; neither procs on his own
     * spells), then Ignite 3; the first talent makes Fire his primary tree, which teaches Flashburn. The Mastery passive
     * (86473) learned and kMasteryRating mastery rating, so his mastery is 8 points plus the rating's. Ignite's dummy
     * aura procs on each Fireball crit and casts 12654 on the Camel with the rank's 40% of the crit's damage as an
     * integer, over two ticks 2 s apart. 12654 takes no damage mods; Flashburn's DoT modifier names it in its class
     * mask. Four Fireball crits: the first three 4.1 s apart, so each Ignite ends before the next crit lands; the
     * fourth kRefreshEveryMs after the third, so it lands between the third Ignite's two ticks and re-applies it with
     * one tick left, which carries its remaining damage (SPELL_ATTR_EX4_STACK_DOT_MODIFIER). The fourth cast waits
     * kLastStepMs, so the re-applied Ignite ticks out before the verdict.
     *
     * `ignite`: per cast, the crit's amount and the 12654 ticks after it, `<crit>/<tick>/...`: two after each of the
     * first two, one after the third, and the re-applied Ignite's after the fourth. With Done(x) the tick x with
     * Flashburn's percent added the way the spell modifiers add a percent, x + x x (2.8 x mastery) / 100 in float,
     * truncated, and share(D) = int(D x 40 / 100) / 2: OK when every cast crit, each tick of the first three casts is
     * Done(share), and the re-applied Ignite's n ticks are all Done(x) for one x whose n ticks hold the bank, the new
     * two ticks of share(D4) and the one tick of share(D3) left, less at most the n - 1 an integer division over n
     * ticks drops. The bank is counted before the mastery step, so the carried tick takes Flashburn once.
     */
    class MageFireIgniteMastery : public MageProcScenario
    {
    public:
        MageFireIgniteMastery() : MageProcScenario("mage-fire-ignite-mastery", 964, "ignite") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        std::vector<uint32> Spells() const override { return { 86473 }; }
        int32 MasteryRating() const override { return kMasteryRating; }

        std::vector<Step> Steps() const override
        {
            Step refreshed = Cast(133, true);
            refreshed.everyMs = kRefreshEveryMs;
            Step last = Cast(133, true);
            last.everyMs = kLastStepMs;
            return { Cast(133, true), Cast(133, true), refreshed, last };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 76595: 49 fields, fnv ab04106a
                { 76595, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBonusCoefficient", 1077097267 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 580542277877760 }, { "eff0.EffectSpellClassMask.Flags2", 1024 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 280 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 amount = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.12654=%u/", &amount) == 1)
                            {
                                ticks.push_back(amount);
                            }
                        }
                    }
                }
            }

            float mastery = 0.0f;
            int32 flashburnAmount = 0;
            float dotPercent = 0.0f;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                mastery = p->GetFloatValue(PLAYER_MASTERY);
                if (Aura const* flashburn = p->GetAura(76595, EFFECT_INDEX_0))
                {
                    flashburnAmount = flashburn->GetModifier()->m_amount;
                }
                if (SpellModMgr* mods = p->GetSpellMods())
                {
                    float probe = 100.0f;
                    mods->ApplySpellMod(12654, SPELLMOD_DOT, probe);
                    dotPercent = probe - 100.0f;
                }
            }
            SpellEntry const* flashburnSpell = sSpellStore.LookupEntry(76595);
            SpellEffectEntry const* flashburnEffect = flashburnSpell ? flashburnSpell->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const float percent = (flashburnEffect ? flashburnEffect->EffectBonusCoefficient : 0.0f) * mastery;
            auto done = [percent](int32 tick) -> uint32
            {
                const float tickValue = float(tick);
                const float withMastery = tickValue + tickValue * percent / 100.0f;
                return withMastery > 0 ? uint32(withMastery) : 0;
            };

            // the ticks each cast owns, in order: two, two, one before the re-application, the rest after it
            const size_t owned[] = { 2, 2, 1, ticks.size() > 5 ? ticks.size() - 5 : 0 };
            std::string value, text;
            bool ok = st.steps.size() == 4 && ticks.size() >= 7;
            char buf[320];
            size_t next = 0;
            int32 shares[4] = { 0, 0, 0, 0 };
            for (uint32 k = 0; k < st.steps.size() && k < 4; ++k)
            {
                Seen const& seen = st.seen[k];
                shares[k] = int32(uint64(seen.amount) * kIgnitePercent / 100) / 2;
                ok = ok && seen.logRead && seen.crit;
                const std::string amount = seen.logRead ? U(seen.amount) : std::string("none");
                std::string own = amount;
                std::string tickText;
                for (size_t i = 0; i < owned[k] && next < ticks.size(); ++i, ++next)
                {
                    own += "/" + U(ticks[next]);
                    tickText += (tickText.empty() ? "" : " ") + U(ticks[next]);
                    if (k < 3)
                    {
                        ok = ok && ticks[next] == done(shares[k]);
                    }
                }
                value += (value.empty() ? "" : ",") + own;
                const std::string want = k < 3 ? ", want " + U(done(shares[k])) : std::string();
                snprintf(buf, sizeof(buf), "cast %u: Fireball %s%s, Ignite's share %d a tick, ticks [%s]%s", k + 1, amount.c_str(),
                         seen.crit ? " crit" : "", shares[k], tickText.c_str(), want.c_str());
                text += (text.empty() ? "" : "; ") + std::string(buf);
            }

            // the re-applied Ignite: its ticks share one value Done(x), and its n ticks of x hold the bank
            const int32 n = int32(owned[3]);
            const int32 bank = 2 * shares[3] + shares[2];
            int32 found = -1;
            if (n >= 2)
            {
                const uint32 tick = ticks[5];
                bool same = true;
                for (size_t i = 5; i < ticks.size(); ++i)
                {
                    same = same && ticks[i] == tick;
                }
                for (int32 x = (bank - n + 1) / n; same && x <= bank / n + 1; ++x)
                {
                    if (n * x <= bank && n * x >= bank - n + 1 && done(x) == tick)
                    {
                        found = x;
                        break;
                    }
                }
            }
            ok = ok && found >= 0;
            const int32 twice = (2 * shares[3] + int32(done(shares[2]))) / (n ? n : 1);
            const std::string fits = found >= 0 ? U(uint32(found)) + ", so " + U(done(found)) : std::string("none fits");
            snprintf(buf, sizeof(buf),
                     "; the re-applied Ignite: %d ticks, bank 2 x %d + %d = %d before the mastery step, a tick %s; a carried tick stepped twice would give about %u",
                     n, shares[3], shares[2], bank, fits.c_str(), done(twice));
            text += buf;
            snprintf(buf, sizeof(buf),
                     "; %u 12654 ticks in all; mastery %.2f, Flashburn's aura amount %d, the DoT percent the spell modifiers give 12654 %.3f, Flashburn's percent from its coefficient %.3f",
                     uint32(ticks.size()), mastery, flashburnAmount, dotPercent, percent);
            text += buf;
            return ok ? value + "(OK: " + text + ")" : "BUG(" + value + ": " + text + ")";
        }

    private:
        static constexpr int32 kMasteryRating = 1000;
        static constexpr uint32 kIgnitePercent = 40;
        static constexpr uint32 kRefreshEveryMs = 3700;
        static constexpr uint32 kLastStepMs = 8000;
    };

    /**
     * S967 `mage-fire-critical-mass`: the proc rows of Critical Mass (11095's, which its rank 12873 takes) and Master of
     * Elements (29074's, which its rank 29075 takes) on a staged Fire mage. Staged: Master of Elements 2 (first, so it
     * names the Fire tree and Pyroblast comes with it), Burning Soul 3, Improved Fire Blast 2, Fire Power 3, Blazing
     * Speed 2, Hot Streak, Improved Scorch 2, Combustion, Molten Shields, Improved Hot Streak 2, Firestarter, Improved
     * Flamestrike 2, Dragon's Breath and Molten Fury 2 (the 25 points the sixth tier asks), then Critical Mass 3; Arcane
     * Blast (30451) learned. Steps: Pyroblast, which must not crit (the crit aura on him at NonCritAuraPoints()), then
     * Arcane Blast, which must crit. The rows name the spells the talents' texts name: Pyroblast's hit applies Critical
     * Mass (22959) to the Camel at rank 3's 100%, and the Arcane Blast crit gives back 30% of its base mana cost through
     * 29077. Without them Critical Mass takes the class mask of its SpellEffect.dbc row, which holds Scorch alone, and
     * Master of Elements takes Fire and Frost crits alone.
     *
     * `criticalMass`: the 22959 holder on the Camel after step 1, read when step 2 runs (Pyroblast's 3.5 s cast and its
     * flight end after step 1's read), and the 29077 refunds after each step; OK when the holder is there and step 2
     * alone gave a refund.
     */
    class MageFireCriticalMass : public MageProcScenario
    {
    public:
        MageFireCriticalMass() : MageProcScenario("mage-fire-critical-mass", 967, "criticalMass") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10545, 1 }, { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 1 }, { 10561, 0 },
                     { 10543, 0 }, { 12121, 1 }, { 11431, 0 }, { 10734, 1 }, { 10571, 0 }, { 10563, 1 }, { 10541, 2 } };
        }
        std::vector<uint32> Spells() const override { return { 30451 }; }

        std::vector<Step> Steps() const override
        {
            return { Cast(11366, false), Cast(30451, true) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 30451: 49 fields, fnv d3aeb596
                { 30451, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 841 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 137 }, { "eff0.EffectRealPointsPerLevel", 1084227584 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 64 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.EffectTriggerSpell", 36032 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 36032: 72 fields, fnv 62406494
                { 36032, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 536875008 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 150 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 14 }, { "eff1.EffectSpellClassMask.Flags", 536870912 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 107 }, { "eff2.EffectBasePoints", -100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 10 }, { "eff2.EffectSpellClassMask.Flags", 536870912 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44448: 49 fields, fnv 2ada6a76
                { 44448, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12873: 72 fields, fnv ab2c2f74
                { 12873, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.EffectTriggerSpell", 22959 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 281474976710656 }, { "eff1.EffectSpellClassMask.Flags2", 128 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 15 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 22 }, { "eff2.EffectSpellClassMask.Flags", 562949953421312 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 22959: 26 fields, fnv dd0d850d
                { 22959, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 179 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 22959; }
        bool WatchedOnTarget() const override { return true; }
        uint32 CastEveryMs() const override { return 4300; }
        int32 NonCritAuraPoints() const override { return -100; }

        std::string Own(St const& st) const override
        {
            const std::string holder = st.seen.size() > 1 && !st.seen[1].watchedBefore.empty() ? st.seen[1].watchedBefore : std::string("none");
            bool ok = holder != "none";
            std::string text;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::string amounts = EnergizeAmounts(k, 29077);
                text += "; 29077 after step " + U(k + 1) + ": " + (amounts.empty() ? std::string("none") : amounts);
                ok = ok && amounts.empty() == (k == 0);
            }
            return std::string(ok ? "OK" : "BUG") + "(22959 on the Camel after step 1: " + holder + text + ")";
        }
    };

    /**
     * S966 `mage-fire-ignite`: Ignite (12846, rank 3 of 3) banks two Fireball crits that overlap, on a staged Fire mage
     * with kSpellPower spell power and no mastery. Staged: Burning Soul 3 and Improved Fire Blast 2 (the five points
     * the second tier asks), then Ignite 3; the first talent makes Fire his primary tree, which teaches Flashburn, but
     * the Mastery passive is not learned, so his mastery is 0 and Flashburn adds nothing. Ignite's dummy aura procs on
     * each Fireball crit and casts 12654 on the Camel with the rank's 40% of the crit's damage as an integer, over the
     * DoT's two ticks, 2 s apart. Two Fireball crits kOverlapEveryMs apart: the second lands between the first Ignite's
     * two ticks and re-applies it with one tick left, so the refreshed Ignite keeps that tick's time, gains the full
     * duration on top and spreads the two banks over its three ticks (SPELL_ATTR_EX4_STACK_DOT_MODIFIER). The second
     * cast waits kLastStepMs, so the refreshed Ignite ticks out before the verdict.
     *
     * `ignite`: the 12654 ticks in every window, `ticks=<n>,sum=<total>,expected=<e>`. With tick(D) = int(D x 40 / 100)
     * / 2 for each recorded crit D and carried = (2 x tick(D2) + tick(D1)) / 3, its remainder r dropped by the integer
     * division: OK when both casts crit, the first tick is tick(D1), the three after the re-application are each
     * carried, and the sum is e = 2 x tick(D1) + 2 x tick(D2) - r, both banks less that remainder alone.
     */
    class MageFireIgnite : public MageProcScenario
    {
    public:
        MageFireIgnite() : MageProcScenario("mage-fire-ignite", 966, "ignite") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step first = Cast(133, true);
            first.everyMs = kOverlapEveryMs;
            Step overlapping = Cast(133, true);
            overlapping.everyMs = kLastStepMs;
            return { first, overlapping };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return 12654; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 amount = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.12654=%u/", &amount) == 1)
                            {
                                ticks.push_back(amount);
                            }
                        }
                    }
                }
            }

            // the DoT's tick count as the client's data gives it: its duration over its period
            SpellEntry const* dot = sSpellStore.LookupEntry(12654);
            SpellEffectEntry const* dotEffect = dot ? dot->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const int32 duration = GetSpellDuration(dot);
            const uint32 period = dotEffect ? dotEffect->EffectAuraPeriod : 0;
            const uint32 dotTicks = period && duration > 0 ? uint32(duration) / period : 0;
            int32 percent = 0;
            int32 spellPower = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                if (Aura const* ignite = p->GetAura(12846, EFFECT_INDEX_0))
                {
                    percent = ignite->GetModifier()->m_amount;
                }
                spellPower = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
            }

            bool ok = st.steps.size() == 2 && dotTicks == 2;
            uint32 tick[2] = { 0, 0 };
            std::string crits;
            for (uint32 k = 0; k < st.steps.size() && k < 2; ++k)
            {
                Seen const& seen = st.seen[k];
                ok = ok && seen.logRead && seen.crit;
                tick[k] = dotTicks ? uint32(uint64(seen.amount) * kIgnitePercent / 100) / dotTicks : 0;
                crits += (crits.empty() ? "" : ", ") + (seen.logRead ? U(seen.amount) : std::string("none")) +
                         (seen.crit ? " crit" : "") + " (a tick of " + U(tick[k]) + ")";
            }
            const uint32 bank = 2 * tick[1] + tick[0];
            const uint32 carried = bank / 3;
            const uint32 dropped = bank % 3;
            const uint32 expected = 2 * tick[0] + 2 * tick[1] - dropped;
            uint32 sum = 0;
            std::string tickText;
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                sum += ticks[i];
                tickText += (tickText.empty() ? "" : " ") + U(ticks[i]);
                ok = ok && ticks[i] == (i == 0 ? tick[0] : carried);
            }
            ok = ok && ticks.size() == 4 && sum == expected;

            const std::string value = "ticks=" + U(ticks.size()) + ",sum=" + U(sum) + ",expected=" + U(expected);
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "Fireball %s; 12654 ticks [%s]: the first Ignite's first tick, then the re-applied Ignite's three of (2 x %u + %u) / 3 = %u, "
                     "%u dropped by the division; %u ticks to an Ignite (%d ms over a period of %u); Ignite's aura amount %d; fire spell power %d",
                     crits.c_str(), tickText.c_str(), tick[1], tick[0], carried, dropped, dotTicks, duration, period,
                     percent, spellPower);
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kIgnitePercent = 40;
        static constexpr uint32 kOverlapEveryMs = 3700;
        static constexpr uint32 kLastStepMs = 9000;
    };

    /**
     * S968 `mage-fire-impact`: Impact's Fire Blast on a staged Fire mage with kSpellPower spell power and no mastery.
     * Staged: Burning Soul 3 and Improved Fire Blast 2 (the five points the second tier asks), then Ignite 3 and Impact 2
     * (12357); Fire Blast (2136) and the Impact buff (64343) learned. Camel 1 stands ten yards in front of him and three
     * more across his front from it (ExtraTargets): Camel 2 four yards to one side and Camel 3 six yards to the other,
     * inside the 12 yards of Impact's script effect around Camel 1, and Camel 4 twenty yards out, beyond them. Before each
     * cast he takes Precision (13843, +6% spell hit), so no cast misses a level-85 Camel. Steps, each read kReadMs after
     * it: Pyroblast at Camel 1, which must crit (its 3.5 s cast and its flight land it about 3.9 s after the step: its
     * DoT and an Ignite on the Camel); the Impact buff on him by an aura cast (the talent's own 10% is not forced);
     * kFireBlastEveryMs later Fire Blast at Camel 1, which must not crit. The buff's proc casts 12355 at Camel 1: its
     * effect 0 stuns Camel 1 for 2 s and its script effect copies his Pyroblast DoT and Ignite from Camel 1 to every other
     * Camel within its radius.
     *
     * `impact`: `stun=<the Camels holding his 12355>,copies=<the Camels besides Camel 1 holding both his 11366 and his
     * 12654>,far=<what Camel 4 holds of his>,buff=<his 64343>,pyro=<Camel 1's 11366 tick>,ignite=<Camel 1's 12654 tick>`,
     * all read at the Fire Blast's read. OK when the Pyroblast crit and the Fire Blast hit without one and cast 12355,
     * Camel 1 alone is stunned, Camels 2 and 3 alone hold the copies, each copy's tick equal to Camel 1's and its time
     * left within one period of Camel 1's, Camel 4 holds nothing of his, and his buff is gone unless the Fire Blast cast
     * it again.
     */
    class MageFireImpact : public MageProcScenario
    {
    public:
        MageFireImpact() : MageProcScenario("mage-fire-impact", 968, "impact") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 10537, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFireBlast, kImpactBuff }; }
        int32 SpellPower() const override { return kSpellPower; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, 4.0f }, { 0.0f, -6.0f }, { 0.0f, 20.0f } };
        }

        std::vector<Step> Steps() const override
        {
            return { Cast(kPyroblast, true), AuraCast(kImpactBuff, kFireBlastEveryMs), Cast(kFireBlast, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 64343: 26 fields, fnv d38ef595
                { 64343, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12355 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12355: 49 fields, fnv 58da00a3
                { 12355, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 53 }, { "eff1.ImplicitTarget_1", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12357: 26 fields, fnv c557c869
                { 12357, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 64343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kImpactBuff; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 CastEveryMs() const override { return kCastEveryMs; }
        uint32 CastReadMs() const override { return kReadMs; }
        int32 NonCritAuraPoints() const override { return -100; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (!atRead || k + 1 != st.steps.size())
            {
                return;
            }
            m_read = true;
            m_camels.assign(st.targets.size(), CamelRead());
            std::string text;
            for (uint32 camel = 0; camel < st.targets.size(); ++camel)
            {
                CamelRead& found = m_camels[camel];
                Creature* unit = Get(st.targets[camel]);
                if (unit)
                {
                    found.stunned = unit->GetSpellAuraHolder(kImpactStun, st.player) != NULL;
                    found.pyro = DotOf(unit, kPyroblast, st.player);
                    found.ignite = DotOf(unit, kIgniteDot, st.player);
                    for (uint32 spell : kHisSpells)
                    {
                        if (unit->GetSpellAuraHolder(spell, st.player))
                        {
                            found.held.push_back(spell);
                        }
                    }
                }
                text += (camel ? "; " : "") + std::string("Camel ") + U(camel + 1) + (unit ? "" : " gone") +
                        (found.stunned ? ", 12355" : "") + ", 11366 " + DotText(found.pyro) + ", 12654 " + DotText(found.ignite);
            }
            m_rec.Note("his auras on each Camel: " + text);
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 3 || !m_read || m_camels.size() != 4)
            {
                return "BUG(the Fire Blast's read did not run)";
            }
            Seen const& pyroblast = st.seen[0];
            Seen const& buff = st.seen[1];
            Seen const& fireBlast = st.seen[2];
            CamelRead const& stunned = m_camels[0];

            std::string stun, copies, farCamel;
            bool copiesMatch = true;
            for (uint32 camel = 0; camel < m_camels.size(); ++camel)
            {
                CamelRead const& found = m_camels[camel];
                if (found.stunned)
                {
                    stun += (stun.empty() ? "" : "+") + U(camel + 1);
                }
                if (camel && found.pyro.held && found.ignite.held)
                {
                    copies += (copies.empty() ? "" : "+") + U(camel + 1);
                    copiesMatch = copiesMatch && SameDot(found.pyro, stunned.pyro) && SameDot(found.ignite, stunned.ignite);
                }
            }
            for (uint32 spell : m_camels[3].held)
            {
                farCamel += (farCamel.empty() ? "" : "+") + U(spell);
            }
            stun = stun.empty() ? "none" : stun;
            copies = copies.empty() ? "none" : copies;
            farCamel = farCamel.empty() ? "none" : farCamel;
            const bool recast = Triggered(fireBlast, kImpactBuff);
            const std::string buffNow = fireBlast.holder == "none" ? "none" : "up";

            const bool ok = pyroblast.logRead && pyroblast.crit && !buff.missed && fireBlast.logRead && !fireBlast.crit &&
                            Triggered(fireBlast, kImpactStun) && stunned.pyro.held && stunned.ignite.held && stun == "1" &&
                            copies == "2+3" && copiesMatch && farCamel == "none" && (buffNow == "none" || recast);
            const std::string value = "stun=" + stun + ",copies=" + copies + ",far=" + farCamel + ",buff=" + buffNow + ",pyro=" +
                                      U(uint32(stunned.pyro.amount)) + ",ignite=" + U(uint32(stunned.ignite.amount));
            char buf[384];
            snprintf(buf, sizeof(buf),
                     "Pyroblast %u%s, Fire Blast %u%s; the Fire Blast's windows cast [%s]; his 64343 at its read: %s",
                     pyroblast.amount, pyroblast.crit ? " crit" : "", fireBlast.amount, fireBlast.crit ? " crit" : "",
                     TriggeredText(fireBlast).c_str(), fireBlast.holder.c_str());
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kImpactBuff = 64343;
        static constexpr uint32 kImpactStun = 12355;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCastEveryMs = 4300;
        static constexpr uint32 kFireBlastEveryMs = 1500;
        static constexpr uint32 kReadMs = 1000;
        /// Every aura of his a Camel may hold here: the stun and the five DoTs Impact copies.
        static constexpr uint32 kHisSpells[] = { 12355, 11366, 92315, 44457, 12654, 83853 };

        /// A periodic damage aura of his on a Camel: its tick, its holder's time left and full duration, its period.
        struct DotRead
        {
            bool held = false;
            int32 amount = 0;
            int32 duration = 0;
            int32 maxDuration = 0;
            int32 period = 0;
        };

        /// What the Fire Blast's read found on one Camel.
        struct CamelRead
        {
            bool stunned = false;
            DotRead pyro;
            DotRead ignite;
            std::vector<uint32> held;           ///< the spells of kHisSpells it holds from him
        };

        bool m_read = false;
        std::vector<CamelRead> m_camels;

        /// The periodic damage aura of `caster`'s holder of `spell` on `unit`.
        static DotRead DotOf(Unit* unit, uint32 spell, ObjectGuid const& caster)
        {
            DotRead dot;
            SpellAuraHolder* holder = unit->GetSpellAuraHolder(spell, caster);
            for (uint32 e = 0; holder && e < MAX_EFFECT_INDEX; ++e)
            {
                Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(e));
                if (aura && aura->GetModifier()->m_auraname == SPELL_AURA_PERIODIC_DAMAGE)
                {
                    dot.held = true;
                    dot.amount = aura->GetModifier()->m_amount;
                    dot.duration = holder->GetAuraDuration();
                    dot.maxDuration = holder->GetAuraMaxDuration();
                    dot.period = int32(aura->GetModifier()->periodictime);
                    break;
                }
            }
            return dot;
        }

        static std::string DotText(DotRead const& dot)
        {
            if (!dot.held)
            {
                return "none";
            }
            char buf[96];
            snprintf(buf, sizeof(buf), "tick %d, left %d of %d, every %d", dot.amount, dot.duration, dot.maxDuration,
                     dot.period);
            return buf;
        }

        /// A copy's tick equals the source's, and its time left is within one period of the source's.
        static bool SameDot(DotRead const& copy, DotRead const& source)
        {
            const int32 apart = copy.duration > source.duration ? copy.duration - source.duration : source.duration - copy.duration;
            return copy.held && source.held && copy.amount == source.amount && apart <= source.period;
        }

        static std::string TriggeredText(Seen const& seen)
        {
            std::string out;
            for (size_t i = 0; i < seen.triggered.size(); ++i)
            {
                out += (i ? "+" : "") + U(seen.triggered[i]);
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * S969 `mage-fire-cauterize`: a killing blow on a staged Fire mage with Cauterize (86949, rank 2 of 2). Staged:
     * Burning Soul 3, Improved Fire Blast 2, Fire Power 3 and Blazing Speed 2 (the ten points the third tier asks;
     * Blazing Speed's hits taken never come), then Cauterize 2. Its aura holds the chance in effect 0 (100) and the
     * health percent in effect 1 (40): a blow that would kill him leaves him at 40% of his maximum health, healed up to
     * it through the heal effect of the follow-up spell 87023, which also burns him for 12% of his maximum health every
     * 1.5 s for 6 s (a periodic damage percent aura that takes no damage mods), and puts 87023 on cooldown on him for
     * 60 s, the lockout between two saves. Steps: his health set to 1 and the Camel's blow of kBlow fire damage; his
     * health set to its maximum 3.5 s later, after the burn's second tick and before its third, so he outlives all
     * four.
     *
     * `cauterize`: `before=<his health before the blow> after=<his health right after it> burn=<the 87023 ticks in the
     * steps' windows, as <count>x<amount> when they are equal> lockout=<on when 87023 was on cooldown on him at the
     * second read>`; OK when they are 1, 40% of his maximum health, four ticks of 12% of it and on, and the first read
     * found the 87023 holder on him with period kBurnPeriod and maximum duration kBurnDuration.
     */
    class MageFireCauterize : public MageProcScenario
    {
    public:
        MageFireCauterize() : MageProcScenario("mage-fire-cauterize", 969, "cauterize") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 11433, 1 } };
        }
        bool TakesDamage() const override { return true; }
        bool Heals() const override { return true; }
        uint32 SwingHealth() const override { return 1; }

        std::vector<Step> Steps() const override
        {
            return { Blow(kBlow, SPELL_SCHOOL_MASK_FIRE, kBlowEveryMs), FullHealth(kFullHealthEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 86949: 49 fields, fnv 7082c5a5
                { 86949, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 40 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 87023: 72 fields, fnv 7c2b9732
                { 87023, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 89 }, { "eff0.EffectAuraPeriod", 1500 }, { "eff0.EffectBasePoints", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kBurn; }
        bool ReadsPeriodic() const override { return true; }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> ticks;
            std::string heals;
            const std::string heal = "heal self.heal." + U(kBurn) + "=";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    const std::string window = "step" + U(k + 1) + suffix;
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_PERIODICAURALOG);
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
                            if (sscanf(fields[f].c_str(), "tick self.tick.87023=%u/", &amount) == 1)
                            {
                                ticks.push_back(amount);
                            }
                        }
                    }
                    std::vector<Recorder::Seen const*> healLogs = m_rec.SeenIn(window, SMSG_SPELLHEALLOG);
                    for (size_t i = 0; i < healLogs.size(); ++i)
                    {
                        if (healLogs[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLHEALLOG, &healLogs[i]->payload[0],
                                                                                 healLogs[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            if (fields[f].find(heal) != std::string::npos)
                            {
                                heals += (heals.empty() ? "" : "; ") + fields[f];
                            }
                        }
                    }
                }
            }

            uint32 maxHealth = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                maxHealth = p->GetMaxHealth();
            }
            const uint32 floorHealth = uint32(uint64(maxHealth) * kFloorPercent / 100);
            const uint32 burnTick = uint32(uint64(maxHealth) * kBurnPercent / 100);

            Seen const& blow = st.seen[0];
            Seen const& full = st.seen[1];
            bool equal = !ticks.empty();
            std::string tickList;
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                equal = equal && ticks[i] == ticks[0];
                tickList += (i ? "/" : "") + U(ticks[i]);
            }
            const std::string burn = ticks.empty() ? std::string("0") : equal ? U(ticks.size()) + "x" + U(ticks[0]) : tickList;
            const std::string value = "before=" + U(blow.healthBefore) + " after=" + U(blow.healthAfter) + " burn=" + burn +
                                      " lockout=" + (full.watchedOnCooldown ? "on" : "off");
            const bool ok = st.steps.size() == 2 && blow.read && full.read && blow.healthBefore == 1 && blow.healthAfter == floorHealth &&
                            blow.periodicRead && blow.period == kBurnPeriod && blow.maxDuration == kBurnDuration &&
                            ticks.size() == kBurnTicks && equal && ticks[0] == burnTick && full.watchedOnCooldown;
            const std::string holder = blow.periodicRead ? "period " + std::to_string(blow.period) + ", duration " +
                                                               std::to_string(blow.duration) + "/" + std::to_string(blow.maxDuration)
                                                         : std::string("none");
            const std::string detail = "his maximum health " + U(maxHealth) + ": the floor " + U(kFloorPercent) + "% is " + U(floorHealth) +
                                       ", a burn tick " + U(kBurnPercent) + "% is " + U(burnTick) + "; the 87023 holder on him at the first read: " +
                                       holder + "; the heal logs of 87023: " + (heals.empty() ? std::string("none") : heals) +
                                       "; 87023 on cooldown at the first read " + (blow.watchedOnCooldown ? "yes" : "no") + ")";
            return ok ? "OK(" + value + "; " + detail : "BUG(" + value + "; " + detail;
        }

    private:
        static constexpr uint32 kBurn = 87023;
        static constexpr uint32 kBlow = 1000000;
        static constexpr uint32 kBlowEveryMs = 3500;
        static constexpr uint32 kFullHealthEveryMs = 5000;
        static constexpr uint32 kFloorPercent = 40;
        static constexpr uint32 kBurnPercent = 12;
        static constexpr int32 kBurnPeriod = 1500;
        static constexpr int32 kBurnDuration = 6000;
        static constexpr size_t kBurnTicks = 4;
    };

    /**
     * S970 `mage-fire-living-bomb-rules`: Living Bomb (44457) cast by a staged mage with no talents who learned the
     * spell, without haste rating, so a bomb lasts its 12 s. Once staged, his Offensive State (79684) is taken off and
     * put back at kOffensiveStatePoints, so the end of each harmful cast of his casts Arcane Missiles! (79683, 20 s) on
     * him; he knows no Hot Streak, so nothing refuses it. Before the first cast he takes Precision (13843, +6% spell
     * hit), so no cast can miss a level-85 Camel. The explosion (44461), which the Camel casts for him when the aura
     * runs out, is one of his hits and starts his procs at its hit, but the end of its cast is the Camel's, so it does
     * not cast 79683 on him again. A bomb whose Camel dies leaves with AURA_REMOVE_BY_DEATH and does not explode.
     * Steps: Living Bomb, left to run out its 12 s; 13 s after it, the Camel's swing at him, which is no spell of his;
     * Living Bomb again; 2 s later he deals the Camel its health as plain damage. A cast is read kReadMs after it; at
     * the swing's read the scenario notes the time 79683 has left on him.
     *
     * `rules`: `expiry=<the 44461 damage logs in the first step's windows> refreshed=<yes when 79683 has more than
     * kRefreshedAboveMs left on him at the swing's read> death=<the 44461 damage logs in the kill's windows, none for
     * none>`; OK at one, no and none, with 79683 on him at the swing's read and the Camel dead at the kill's read.
     * Cast at the first bomb's cast alone, 79683 has about 6.7 s left at that read; cast again at the explosion, 1.3 s
     * before it, it would have about 18.7 s.
     */
    class MageFireLivingBombRules : public MageProcScenario
    {
    public:
        MageFireLivingBombRules() : MageProcScenario("mage-fire-living-bomb-rules", 970, "rules") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return std::vector<std::pair<uint32, uint32> >(); }
        std::vector<uint32> Spells() const override { return { kLivingBomb }; }
        bool TakesDamage() const override { return true; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 CasterAura() const override { return kPrecision; }

        void AfterStage(Player& p) override
        {
            p.RemoveAurasDueToSpell(kOffensiveState);
            int32 points = kOffensiveStatePoints;
            p.CastCustomSpell(&p, kOffensiveState, &points, NULL, NULL, true);
            Aura const* offensiveState = p.GetAura(kOffensiveState, EFFECT_INDEX_0);
            m_rec.Note("his " + U(kOffensiveState) + " taken off and cast again at " + U(uint32(kOffensiveStatePoints)) + ": " +
                       (offensiveState ? "its effect 0 holds " + std::to_string(offensiveState->GetModifier()->m_amount)
                                       : std::string("not on him")));
        }

        std::vector<Step> Steps() const override
        {
            return { AuraCast(kLivingBomb, kExpiryEveryMs), Swing(), AuraCast(kLivingBomb, kKillEveryMs), Kill(kKillEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kArcaneMissilesBuff; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            if (!atRead || k != kSwingStep)
            {
                return;
            }
            SpellAuraHolder const* holder = p->GetSpellAuraHolder(kArcaneMissilesBuff);
            st.seen[k].reads["left"] = holder ? holder->GetAuraDuration() : -1;
            m_rec.Note(U(kArcaneMissilesBuff) + " on him: " + (holder ? std::to_string(holder->GetAuraDuration()) + " ms left of " +
                                                                  std::to_string(holder->GetAuraMaxDuration())
                                                                : std::string("none")));
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> hits(st.steps.size(), 0);
            std::string logs;
            const std::string hit = ".hit." + U(kExplosion) + "=";
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (std::string const& window : StepWindows(k))
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            if (fields[f].find(hit) != std::string::npos)
                            {
                                ++hits[k];
                                logs += (logs.empty() ? "" : "; ") + U(k + 1) + " " + fields[f];
                            }
                        }
                    }
                }
            }

            Seen const& swing = st.seen[kSwingStep];
            Seen const& kill = st.seen[kKillStep];
            std::map<std::string, int64>::const_iterator left = swing.reads.find("left");
            const int64 leftMs = left == swing.reads.end() ? -1 : left->second;
            const bool refreshed = swing.read && leftMs > kRefreshedAboveMs;
            const bool dead = kill.read && kill.healthAfter == 0;
            const std::string value = "expiry=" + U(hits[kExpiryStep]) + " refreshed=" + (refreshed ? "yes" : "no") + " death=" +
                                      (hits[kKillStep] ? U(hits[kKillStep]) : std::string("none"));
            const bool ok = hits[kExpiryStep] == 1 && swing.read && leftMs > 0 && !refreshed && hits[kKillStep] == 0 &&
                            dead;
            const std::string detail = U(kArcaneMissilesBuff) + " on him at the swing's read: " + swing.holder + ", " +
                                       std::to_string(leftMs) + " ms left; the Camel's health at the kill's read " +
                                       U(kill.healthAfter) + "; the " + U(kExplosion) + " damage logs by step: " +
                                       (logs.empty() ? std::string("none") : logs);
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kExplosion = 44461;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kArcaneMissilesBuff = 79683;
        static constexpr int32 kOffensiveStatePoints = 100;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kExpiryEveryMs = 13000;
        static constexpr uint32 kKillEveryMs = 2000;
        static constexpr int64 kRefreshedAboveMs = 15000;
        static constexpr size_t kSteps = 4;
        static constexpr uint32 kExpiryStep = 0;
        static constexpr uint32 kSwingStep = 1;
        static constexpr uint32 kKillStep = 3;
    };

    /**
     * S942 `mage-offensive-state-damageless`: Offensive State (79684) and Impact (12357) on a staged Fire mage, at the
     * casts and hits of spells that deal no damage. Staged as 968 stages it (Burning Soul 3, Improved Fire Blast 2,
     * Ignite 3 and Impact 2: no Hot Streak, so nothing refuses Arcane Missiles!); Slow (31589), Polymorph (118) and
     * Living Bomb (44457) learned. Once staged, his Offensive State is taken off and put back at kOffensiveStatePoints,
     * so the end of each harmful cast of his, which the proc accepts, casts Arcane Missiles! (79683) on him; Impact
     * keeps its own 10% at a hit. Before each cast he takes Precision (13843, +6% spell hit), so no cast misses a
     * level-85 Camel but the one meant to. Steps: Slow twice and Polymorph twice at the Camel, each a hit; a wait, at
     * whose start the Camel casts Wily Wits (75533) on itself, an aura that makes the next spell cast at it miss; a
     * Slow, which misses; then kBombCasts Living Bombs, kEveryMs apart, so each lands on the bomb before it and no bomb
     * runs out. A cast is read kReadMs after it. The Camel takes him as its victim at the first step, so when the first
     * bomb's tick breaks its Polymorph it turns on him and does not run home and evade the next bomb.
     *
     * `damageless`: `am=<the steps among the four hits at whose read Arcane Missiles! (79683) stood on him>,miss=<the
     * same for the Slow that missed>,veto=<the steps among the five Slow and Polymorph steps at whose read the Impact
     * buff (64343) stood on him>,bomb=<the same over the Living Bomb steps>`; both buffs are taken off him at each
     * read, so each step's count is its own. OK at four, one, none and one or more, and the last Slow listed as
     * missed. The end of each cast of a spell with no damage effect rolls Arcane Missiles!, the Slow that misses
     * included, and its hit never rolls Impact, nor does the miss; Living Bomb's cast rolls both. The rolls of a step
     * are seeded, so the counts are the same each run. The count of Impact buffs can read 0 even when Impact does not
     * refuse these spells (each roll is 10% a hit), so the unit test
     * MageScripts_ImpactRollsOnlyFromSpellsThatDealDamage proves the refusal.
     */
    class MageOffensiveStateDamageless : public MageProcScenario
    {
    public:
        MageOffensiveStateDamageless() : MageProcScenario("mage-offensive-state-damageless", 942, "damageless") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 10537, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kSlow, kPolymorph, kLivingBomb }; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 CastReadMs() const override { return kReadMs; }

        void AfterStage(Player& p) override
        {
            p.RemoveAurasDueToSpell(kOffensiveState);
            int32 points = kOffensiveStatePoints;
            p.CastCustomSpell(&p, kOffensiveState, &points, NULL, NULL, true);
            Aura const* offensiveState = p.GetAura(kOffensiveState, EFFECT_INDEX_0);
            m_rec.Note("his " + U(kOffensiveState) + " taken off and cast again at " + U(uint32(kOffensiveStatePoints)) + ": " +
                       (offensiveState ? "its effect 0 holds " + std::to_string(offensiveState->GetModifier()->m_amount)
                                       : std::string("not on him")));
        }

        std::vector<Step> Steps() const override
        {
            Step missed = AuraCast(kSlow, kEveryMs);
            missed.refusal = true;
            std::vector<Step> steps = { AuraCast(kSlow, kEveryMs), AuraCast(kSlow, kEveryMs), AuraCast(kPolymorph, kEveryMs),
                                        AuraCast(kPolymorph, kEveryMs), Wait(kWaitMs), missed };
            for (uint32 i = 0; i < kBombCasts; ++i)
            {
                steps.push_back(AuraCast(kLivingBomb, kEveryMs));
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 31589: 72 fields, fnv b6a65259
                { 31589, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.EffectSpellClassMask.Flags", 14684919 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 218 }, { "eff1.EffectBasePoints", -60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 12589815 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 216 }, { "eff2.EffectBasePoints", -30 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectSpellClassMask.Flags", 4194437 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 75533: 26 fields, fnv 5234ff32
                { 75533, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 186 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12355: 49 fields, fnv 58da00a3
                { 12355, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 53 }, { "eff1.ImplicitTarget_1", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12357: 26 fields, fnv c557c869
                { 12357, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 64343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 64343: 26 fields, fnv d38ef595
                { 64343, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12355 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                st.seen[k].reads["am"] = p->HasAura(kArcaneMissilesBuff) ? 1 : 0;
                st.seen[k].reads["impact"] = p->HasAura(kImpactBuff) ? 1 : 0;
                p->RemoveAurasDueToSpell(kArcaneMissilesBuff);
                p->RemoveAurasDueToSpell(kImpactBuff);
                if (k == kMissStep && target)
                {
                    target->RemoveAurasDueToSpell(kWilyWits);
                }
                return;
            }
            if (k == 0 && target)
            {
                // the first Living Bomb tick breaks the Polymorph; a Camel with no victim would run home then and
                // evade the next bomb (Unit::SetConfused, Creature::IsInEvadeMode)
                const bool attacking = target->Attack(p, false);
                m_rec.Note(std::string("the Camel takes him as its victim: ") + (attacking ? "it does" : "refused"));
            }
            if (k != kWaitStep || !target)
            {
                return;
            }
            target->CastSpell(target, kWilyWits, true);
            m_rec.Note("the Camel cast " + U(kWilyWits) + " on itself");
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            uint32 am = 0, veto = 0, bomb = 0;
            for (uint32 k = 0; k < kSteps; ++k)
            {
                if (k == kWaitStep)
                {
                    continue;
                }
                if (k < kHitSteps)
                {
                    am += Read(st.seen[k], "am");
                }
                if (k < kFirstBombStep)
                {
                    veto += Read(st.seen[k], "impact");
                }
                else
                {
                    bomb += Read(st.seen[k], "impact");
                }
            }
            const Seen& missedSlow = st.seen[kMissStep];
            const uint32 miss = Read(missedSlow, "am");
            const std::string value = "am=" + U(am) + ",miss=" + U(miss) + ",veto=" + U(veto) + ",bomb=" + U(bomb);
            const bool ok = am == kHitSteps && miss == 1 && veto == 0 && bomb >= 1 && missedSlow.missed;
            const std::string detail = std::string("the last Slow ") + (missedSlow.missed ? "missed" : "hit") +
                                       "; each count is the steps at whose read the buff stood on him, taken off at each read";
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

    private:
        static constexpr uint32 kSlow = 31589;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kOffensiveState = 79684;
        static constexpr uint32 kArcaneMissilesBuff = 79683;
        static constexpr uint32 kImpactBuff = 64343;
        static constexpr uint32 kWilyWits = 75533;
        static constexpr int32 kOffensiveStatePoints = 100;
        static constexpr uint32 kReadMs = 2500;
        static constexpr uint32 kEveryMs = 3000;
        static constexpr uint32 kWaitMs = 3000;
        static constexpr uint32 kBombCasts = 12;
        static constexpr uint32 kHitSteps = 4;
        static constexpr uint32 kWaitStep = 4;
        static constexpr uint32 kMissStep = 5;
        static constexpr uint32 kFirstBombStep = 6;
        static constexpr size_t kSteps = kFirstBombStep + kBombCasts;

        static uint32 Read(Seen const& seen, char const* key)
        {
            std::map<std::string, int64>::const_iterator found = seen.reads.find(key);
            return found != seen.reads.end() && found->second ? 1u : 0u;
        }
    };

    /**
     * S972 `mage-fire-pyromaniac`: Pyromaniac (34295, rank 2 of 2, talent 10559) on a staged Fire mage: "Increases spell
     * haste by $s1% if 3 or more targets are taking Fire damage over time from your spells." Staged as 967 stages its
     * tree: Master of Elements 2, Burning Soul 3, Improved Fire Blast 2, Fire Power 3, Blazing Speed 2, Hot Streak
     * (which keeps Offensive State from casting Arcane Missiles!), Improved Scorch 2, Combustion, Molten Shields,
     * Improved Hot Streak 2, Firestarter, Improved Flamestrike 2, Dragon's Breath and Molten Fury 2 (the 25 points the
     * sixth tier asks), then Pyromaniac 2; Living Bomb (44457) learned, and no haste rating, so a bomb lasts its 12 s.
     * Three Camels stand kTargetSpacing apart, beyond the explosion's 10 yards from one another. Before the first cast he
     * takes Precision (13843, +6% spell hit), so no cast can miss a level-85 Camel. Steps: a Living Bomb at Camels 1, 2
     * and 3, 2 s apart, each read kReadMs after its cast; then, 10 s after the third, when Camel 1's bomb has run out
     * and Camel 2's runs out, the Camel's swing at him, read kSwingReadMs after: one bomb burns then.
     *
     * `pyromaniac`: Pyromaniac's haste (83582) on him at each read, `none` or `83582:<its amount>`; OK when it reads
     * `none,none,83582:<34295's effect 0 value>,none`, and at the third read his cast-speed and cast-haste fields are
     * the staged ones over one plus that value in percent, and at the fourth read the staged ones again.
     */
    class MageFirePyromaniac : public MageProcScenario
    {
    public:
        MageFirePyromaniac() : MageProcScenario("mage-fire-pyromaniac", 972, "pyromaniac") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10545, 1 }, { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 1 }, { 10561, 0 },
                     { 10543, 0 }, { 12121, 1 }, { 11431, 0 }, { 10734, 1 }, { 10571, 0 }, { 10563, 1 }, { kPyromaniacTalent, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kLivingBomb }; }
        bool TakesDamage() const override { return true; }
        uint32 TargetCount() const override { return 3; }
        uint32 CastReadMs() const override { return kReadMs; }
        uint32 CasterAura() const override { return kPrecision; }

        std::vector<Step> Steps() const override
        {
            return { AuraCastAt(kLivingBomb, 0, 2000), AuraCastAt(kLivingBomb, 1, 2000), AuraCastAt(kLivingBomb, 2, 10000), Swing() };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 34295: 26 fields, fnv fb66c962
                { 34295, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83582: 26 fields, fnv fc014937
                { 83582, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 65 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44448: 49 fields, fnv 2ada6a76
                { 44448, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kHaste; }

        std::string Own(St const& st) const override
        {
            SpellEntry const* rank = sSpellStore.LookupEntry(kPyromaniacRank2);
            const int32 haste = rank ? rank->CalculateSimpleValue(EFFECT_INDEX_0) : 0;
            const std::string hasted = U(kHaste) + ":" + std::to_string(haste);
            const std::string wanted[] = { "none", "none", hasted, "none" };
            bool ok = haste > 0 && st.seen.size() == sizeof(wanted) / sizeof(wanted[0]);

            std::string value, text;
            char buf[256];
            for (uint32 k = 0; k < st.seen.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const std::string at = !seen.read ? std::string("unread")
                                       : seen.holder == "none" ? std::string("none")
                                       : U(kHaste) + ":" + std::to_string(seen.watchedAmount);
                value += (k ? "," : "") + at;
                ok = ok && k < sizeof(wanted) / sizeof(wanted[0]) && at == wanted[k];
                snprintf(buf, sizeof(buf), "; read %u: %u on him %s, cast-speed field %.6f, cast-haste field %.6f", k + 1, kHaste,
                         seen.holder.c_str(), seen.castSpeed, seen.castHaste);
                text += buf;
            }

            // his fields under the haste: each staged one times 100 / (100 + the haste), as Unit::ApplyCastTimePercentMod
            // applies a positive percent
            const float speedHasted = st.stagedCastSpeed * 100.0f / (100.0f + float(haste));
            const float hasteHasted = st.stagedCastHaste * 100.0f / (100.0f + float(haste));
            if (ok)
            {
                Seen const& lit = st.seen[kLitRead];
                Seen const& out = st.seen[kOutRead];
                ok = std::fabs(lit.castSpeed - speedHasted) < kFieldTolerance && std::fabs(lit.castHaste - hasteHasted) < kFieldTolerance &&
                     std::fabs(out.castSpeed - st.stagedCastSpeed) < kFieldTolerance &&
                     std::fabs(out.castHaste - st.stagedCastHaste) < kFieldTolerance;
            }
            snprintf(buf, sizeof(buf),
                     "%u's effect 0 value %d; staged with cast-speed field %.6f and cast-haste field %.6f, under the haste %.6f and %.6f",
                     kPyromaniacRank2, haste, st.stagedCastSpeed, st.stagedCastHaste, speedHasted, hasteHasted);
            return value + (ok ? "(OK: " : "(BUG: ") + buf + text + ")";
        }

    private:
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kHaste = 83582;
        static constexpr uint32 kPyromaniacRank2 = 34295;
        static constexpr uint32 kPyromaniacTalent = 10559;
        static constexpr uint32 kReadMs = 1000;
        static constexpr uint32 kLitRead = 2;
        static constexpr uint32 kOutRead = 3;
        static constexpr float kFieldTolerance = 0.00001f;
    };

    /**
     * S984 `mage-fire-glyph-of-frostfire`: Glyph of Frostfire (591, its aura 61205) on a staged Fire mage with
     * kSpellPower spell power, 1197 with his intellect's, and thirteen talent points: Burning Soul 3, which names the
     * Fire tree and so teaches Fire Specialization (+25% fire damage), Improved Fire Blast 2, Fire Power 3, Blazing
     * Speed 2, Blast Wave and Improved Scorch 2. He learns no Mastery passive, so his mastery adds nothing, and
     * Frostfire Bolt (44614) is learned. The glyph adds 15% to the bolt's hit and 3000 ms to its holder (12000 ms in
     * all); its effect 2 (3) is the percent of the hit the bolt's DoT deals over the holder's ticks, a further bolt
     * adds a stack up to the client data's three and a fourth keeps three and rewrites the DoT with its own hit, and
     * the bolt does not slow. Before each cast he takes Precision (13843, +6% spell hit), so no cast misses a level-85
     * Camel, and each cast puts Increased Critical Spell on him at -100, so no hit or tick crits. Steps: four bolts,
     * kBoltEveryMs apart, each read kBoltReadMs after its cast (its 2.5 s cast and its flight land it about 2.8 s after
     * the step, before its read and before the next cast); the fourth has kLastStepMs to the end, past the holder's
     * 12 s. Each read notes the holder's slow (effect 0) and periodic damage (effect 2).
     *
     * `frostfire`: `stacks=<s1>,<s2>,<s3>,<s4>,eff=<m1>/<m2>/<m3>/<m4>,slow=<the slow's amounts at the reads, or
     * absent>,hits=<h1>+<h2>+<h3>+<h4>,ticks=<after the first hit>/<after the second>/<after the third>/<after the
     * fourth>`, the masks those of the holder's effects that hold an aura, the hits each the damage dealt and absorbed,
     * each tick group its amounts with their count; OK when the holder reads stacks 1, 2, 3 and 3 with its periodic
     * damage and without its slow (0x4) and no slow, each hit is no crit and lies in (782..996 + int(1197 x 0.977) =
     * 1169) x 1.2875 x 1.15 = 2888..3205, no tick crits, and the ticks after hit n are each min(n, 3) x int(h_n x
     * <61205's effect 2> / 100) / <ticks>, where <ticks> is the holder's full duration over its period at the fourth
     * read (12000 / 3000 = 4): one after each of the first three hits and <ticks> after the fourth. BUG with the value
     * in the detail otherwise.
     */
    class MageFireGlyphOfFrostfire : public MageProcScenario
    {
    public:
        MageFireGlyphOfFrostfire() : MageProcScenario("mage-fire-glyph-of-frostfire", 984, "frostfire") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostfireSlot }; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step first = Cast(kFrostfireBolt, false);
            first.everyMs = kBoltEveryMs;
            Step second = first;
            Step third = first;
            Step fourth = first;
            fourth.everyMs = kLastStepMs;
            return { first, second, third, fourth };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11367: 26 fields, fnv 5d9994de
                { 11367, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kFrostfireBolt; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kBoltReadMs; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            Seen& seen = st.seen[k];
            SpellAuraHolder* holder = target->GetSpellAuraHolder(kFrostfireBolt);
            uint32 mask = 0;
            for (uint32 e = 0; holder && e < MAX_EFFECT_INDEX; ++e)
            {
                mask |= holder->GetAuraByEffectIndex(SpellEffectIndex(e)) ? 1u << e : 0u;
            }
            Aura* slow = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
            Aura* dot = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_2) : NULL;
            seen.reads["mask"] = mask;
            seen.reads["stack"] = holder ? holder->GetStackAmount() : 0;
            if (slow)
            {
                seen.reads["slow"] = slow->GetModifier()->m_amount;
            }
            m_rec.Note("44614 on the Camel: " + HolderOf(target, kFrostfireBolt) + "; its effect 0 (the slow) " +
                       (slow ? std::to_string(slow->GetModifier()->m_amount) : std::string("absent")) +
                       "; its effect 2 (the periodic damage) " +
                       (dot ? std::to_string(dot->GetModifier()->m_amount) : std::string("absent")));
        }

        std::string Own(St const& st) const override
        {
            // the bolt's hits, each the damage dealt and absorbed, and the DoT's ticks, in the order the record holds
            // them; ticks[n] are the ticks after the nth hit
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> hits;
            std::vector<std::vector<uint32> > ticks(1);
            bool hitCrit = false;
            bool tickCrit = false;
            std::vector<Recorder::Seen> const& packets = m_rec.Packets();
            for (size_t i = 0; i < packets.size(); ++i)
            {
                Recorder::Seen const& packet = packets[i];
                const bool hit = packet.opcode == SMSG_SPELLNONMELEEDAMAGELOG;
                if ((!hit && packet.opcode != SMSG_PERIODICAURALOG) || packet.payload.empty())
                {
                    continue;
                }
                const std::vector<std::string> fields = Trace::CombatLogFields(packet.opcode, &packet.payload[0], packet.payload.size(), roles);
                for (size_t f = 0; f < fields.size(); ++f)
                {
                    uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                    const int parsed = hit ? sscanf(fields[f].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted)
                                           : sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted);
                    if (parsed != 5 || spell != kFrostfireBolt)
                    {
                        continue;
                    }
                    if (hit)
                    {
                        hits.push_back(amount + absorbed);
                        hitCrit = hitCrit || crit != 0;
                        ticks.push_back(std::vector<uint32>());
                    }
                    else
                    {
                        ticks.back().push_back(amount);
                        tickCrit = tickCrit || crit != 0;
                    }
                }
            }

            // the share's percent from the glyph's row, and the holder's tick count over its full duration at the
            // fourth read
            SpellEntry const* glyph = sSpellStore.LookupEntry(kGlyphOfFrostfire);
            const int32 percent = glyph ? glyph->CalculateSimpleValue(EFFECT_INDEX_2) : 0;
            Seen const& last = st.seen[kBolts - 1];
            const uint32 fullTicks = last.periodicRead && last.period > 0 ? uint32(last.maxDuration / last.period) : 0;

            std::string stacks, masks, slows, hitText, tickText, expectedText;
            bool ok = hits.size() == kBolts && !hitCrit && !tickCrit && ticks.size() == kBolts + 1 && ticks[0].empty() &&
                      percent > 0 && fullTicks > 0;
            for (uint32 k = 0; k < kBolts; ++k)
            {
                const int64 stack = Kept(st, k, "stack");
                const int64 mask = Kept(st, k, "mask");
                const int64 slow = Kept(st, k, "slow");
                stacks += (k ? "," : "") + std::to_string(stack);
                char maskText[20];
                snprintf(maskText, sizeof(maskText), "0x%llx", (unsigned long long)(mask < 0 ? 0 : mask));
                masks += (k ? "/" : "") + std::string(maskText);
                slows += (k ? "/" : "") + (slow == kNone ? std::string("absent") : std::to_string(slow));
                ok = ok && stack == int64(std::min(k + 1, kMaxStacks)) && mask == int64(1u << EFFECT_INDEX_2) && slow == kNone;
            }
            for (size_t n = 0; n < hits.size(); ++n)
            {
                hitText += (n ? "+" : "") + U(hits[n]);
                ok = ok && InBand(hits[n], kHitLow, kHitHigh);
            }
            for (size_t n = 1; n < ticks.size(); ++n)
            {
                const uint32 expected = percent > 0 && fullTicks ? std::min(uint32(n), kMaxStacks) *
                                                                       (hits[n - 1] * uint32(percent) / 100 / fullTicks) : 0;
                const size_t wanted = n < kBolts ? 1 : fullTicks;
                tickText += (n > 1 ? "/" : "") + TickGroup(ticks[n]);
                expectedText += (n > 1 ? "/" : "") + U(expected) + "x" + U(wanted);
                ok = ok && ticks[n].size() == wanted;
                for (size_t t = 0; t < ticks[n].size(); ++t)
                {
                    ok = ok && ticks[n][t] == expected;
                }
            }

            const std::string value = "stacks=" + stacks + ",eff=" + masks + ",slow=" + slows + ",hits=" +
                                      (hitText.empty() ? std::string("none") : hitText) + ",ticks=" +
                                      (tickText.empty() ? std::string("none") : tickText);
            const std::string detail = "each hit dealt and absorbed in " + U(kHitLow) + ".." + U(kHitHigh) + ", no crit; the ticks after hit n " +
                                       "min(n, 3) x int(h_n x " + std::to_string(percent) + " / 100) / " + U(fullTicks) + ": " +
                                       (expectedText.empty() ? std::string("none") : expectedText) + "; ticks before the first hit " +
                                       U(ticks[0].size()) + (tickCrit ? "; a tick crit" : "") + (hitCrit ? "; a hit crit" : "") +
                                       "; the holder at the fourth read " + last.holder + ", its periodic effect period " +
                                       std::to_string(last.period) + ", duration " + std::to_string(last.duration) + "/" +
                                       std::to_string(last.maxDuration) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfire = 61205;
        static constexpr uint32 kGlyphOfFrostfireSlot = 591;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kBoltEveryMs = 3300;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kLastStepMs = 15500;
        static constexpr uint32 kBolts = 4;
        static constexpr uint32 kMaxStacks = 3;
        static constexpr uint32 kHitLow = 2888;
        static constexpr uint32 kHitHigh = 3205;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// Tick amounts as `<amount>x<count>` for each amount in the order it first came, `+`-joined; `none` for none.
        static std::string TickGroup(std::vector<uint32> const& amounts)
        {
            std::vector<std::pair<uint32, uint32> > counted;
            for (size_t i = 0; i < amounts.size(); ++i)
            {
                size_t j = 0;
                while (j < counted.size() && counted[j].first != amounts[i])
                {
                    ++j;
                }
                if (j == counted.size())
                {
                    counted.push_back(std::make_pair(amounts[i], 0u));
                }
                ++counted[j].second;
            }
            std::string out;
            for (size_t j = 0; j < counted.size(); ++j)
            {
                out += (j ? "+" : "") + U(counted[j].first) + "x" + U(counted[j].second);
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * S977 `mage-fire-frostfire-snare-immune`: 984's staging without Glyph of Frostfire, against a Camel that holds
     * Hand of Freedom (1044), whose second effect makes it immune to the snare mechanic. His Frostfire Bolt (44614)
     * hits it, but its slow never reaches the holder, and without the glyph the bolt has no DoT of its own: the holder
     * is left with the periodic damage alone and runs out at the unit's next update. One bolt; the read comes
     * kBoltReadMs after the cast, one update after the hit, and the step runs on past the time the first tick would
     * come.
     *
     * `snareimmune`: `immune=<1 when the Camel holds Hand of Freedom at the read>,held=<1 when it holds 44614 of his>,
     * hits=<the hit, dealt and absorbed>,ticks=<the count of the bolt's periodic damage in the record>`; OK when the
     * Camel is immune, holds no 44614 at the read, the hit is no crit and lies in (782..996 + int(1197 x 0.977) = 1169)
     * x 1.2875 x 1.0 = 2511..2787 (the glyph's +15% absent) and no tick came. BUG with the value in the detail
     * otherwise.
     */
    class MageFireFrostfireSnareImmune : public MageProcScenario
    {
    public:
        MageFireFrostfireSnareImmune() : MageProcScenario("mage-fire-frostfire-snare-immune", 977, "snareimmune") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt }; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kEndMs;
            return { bolt };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 1044: 49 fields, fnv 77cb6288
                { 1044, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 77 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 11 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11367: 26 fields, fnv 5d9994de
                { 11367, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        uint32 Watched() const override { return kFrostfireBolt; }
        bool WatchedOnTarget() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kBoltReadMs; }
        uint32 TargetAura() const override { return kHandOfFreedom; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            Seen& seen = st.seen[k];
            seen.reads["immune"] = target->HasAura(kHandOfFreedom) ? 1 : 0;
            seen.reads["held"] = target->GetSpellAuraHolder(kFrostfireBolt) ? 1 : 0;
            m_rec.Note("44614 on the Camel: " + HolderOf(target, kFrostfireBolt) + "; Hand of Freedom (1044) " +
                       HolderOf(target, kHandOfFreedom));
        }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> hits;
            uint32 ticks = 0;
            bool hitCrit = false;
            std::vector<Recorder::Seen> const& packets = m_rec.Packets();
            for (size_t i = 0; i < packets.size(); ++i)
            {
                Recorder::Seen const& packet = packets[i];
                const bool hit = packet.opcode == SMSG_SPELLNONMELEEDAMAGELOG;
                if ((!hit && packet.opcode != SMSG_PERIODICAURALOG) || packet.payload.empty())
                {
                    continue;
                }
                const std::vector<std::string> fields = Trace::CombatLogFields(packet.opcode, &packet.payload[0], packet.payload.size(), roles);
                for (size_t f = 0; f < fields.size(); ++f)
                {
                    uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                    const int parsed = hit ? sscanf(fields[f].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted)
                                           : sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted);
                    if (parsed != 5 || spell != kFrostfireBolt)
                    {
                        continue;
                    }
                    if (hit)
                    {
                        hits.push_back(amount + absorbed);
                        hitCrit = hitCrit || crit != 0;
                    }
                    else
                    {
                        ++ticks;
                    }
                }
            }

            const int64 immune = Kept(st, 0, "immune");
            const int64 held = Kept(st, 0, "held");
            const bool ok = hits.size() == 1 && !hitCrit && InBand(hits[0], kHitLow, kHitHigh) && ticks == 0 && immune == 1 && held == 0;
            const std::string value = "immune=" + std::to_string(immune) + ",held=" + std::to_string(held) + ",hits=" +
                                      (hits.empty() ? std::string("none") : U(hits[0])) + ",ticks=" + U(ticks);
            const std::string detail = "the hit dealt and absorbed in " + U(kHitLow) + ".." + U(kHitHigh) + ", no crit; " +
                                       std::to_string(hits.size()) + " hits" + (hitCrit ? "; a hit crit" : "") + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kHandOfFreedom = 1044;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kEndMs = 7000;
        static constexpr uint32 kHitLow = 2511;
        static constexpr uint32 kHitHigh = 2787;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }
    };

    /**
     * What the Fire passive scenarios share (985-987): a staged Fire mage with kSpellPower spell power, 1197 with his
     * intellect's, and FireTalents(), the 25 points the sixth tier asks, row by row: Burning Soul 3, Improved Fire
     * Blast 2 and Master of Elements 2; Fire Power 3, Blazing Speed 2 and Ignite 3; Blast Wave and Improved Scorch 2;
     * Molten Shields, Combustion and Firestarter; Molten Fury 2, Dragon's Breath and Improved Flamestrike 1. Burning
     * Soul names the Fire tree, which teaches Fire Specialization (+25% fire damage) and Pyroblast. Master of Elements
     * and Ignite proc on crits alone, and no cast of these scenarios crits: each cast puts Increased Critical Spell on
     * him at -100. Before each cast he takes Precision (13843, +6% spell hit), so no cast misses a level-85 Camel.
     */
    class MageFirePassiveScenario : public MageProcScenario
    {
    public:
        MageFirePassiveScenario(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kBlastWave = 11113;
        static constexpr uint32 kCriticalMassDebuff = 22959;

        static std::vector<std::pair<uint32, uint32> > FireTalents()
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10529, 2 }, { 10551, 0 }, { 10547, 1 },
                     { 10543, 0 }, { 10561, 0 }, { 11431, 0 }, { 10563, 1 }, { 10571, 0 }, { 10734, 0 } };
        }

        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }

        /// The prints every Fire passive scenario needs: the holders on him that can proc and what they cast, Precision
        /// and the crit aura; `own` are the scenario's casts and what they reach.
        static std::vector<SpellPrint> PassivePrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 31643: 72 fields, fnv b499aeb8
                { 31643, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 108 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 11 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        /// Critical Mass rank 3 (12873) and the debuff its proc casts (22959).
        static std::vector<SpellPrint> CriticalMassPrints()
        {
            return {
                // 12873: 72 fields, fnv ab2c2f74
                { 12873, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 16 }, { "eff0.EffectTriggerSpell", 22959 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", 15 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 281474976710656 }, { "eff1.EffectSpellClassMask.Flags2", 128 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 108 }, { "eff2.EffectBasePoints", 15 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 22 }, { "eff2.EffectSpellClassMask.Flags", 562949953421312 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 22959: 26 fields, fnv dd0d850d
                { 22959, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 179 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// A read kept by ReadOwn for step k, -1 when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// A cast step's damage as the record read it, or `none`.
        static std::string AmountText(St const& st, uint32 k)
        {
            return k < st.seen.size() && st.seen[k].logRead ? U(st.seen[k].amount) : std::string("none");
        }

        /// Whether cast step k logged a hit in [low, high] (InBand).
        static bool HitInBand(St const& st, uint32 k, uint32 low, uint32 high)
        {
            return k < st.seen.size() && st.seen[k].logRead && !st.seen[k].crit && InBand(st.seen[k].amount, low, high);
        }

    private:
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kPrecision = 13843;
    };

    /**
     * S985 `mage-fire-damage-passives`: the Fire passives that change a hit's damage, a cooldown and a debuff, on a
     * Fire mage staged with FireTalents() and then Critical Mass 3 (28 points); Fire Blast (2136) and Frostfire Bolt
     * (44614) learned. Fire Specialization and Fire Power 3 multiply every fire spell's done damage by 1.25 x 1.03 =
     * 1.2875 (aura 79 by school mask, so Frostfire Bolt's fire-and-frost school takes it too); a spell's spell power
     * share is int(1197 x its coefficient). Steps, every cast one that must not crit:
     *  1. Fire Blast: (955..1131 + int(1197 x 0.429) = 513) x 1.2875 = 1890..2116;
     *  2. Fire Blast again 4.1 s later: refused, its 8 s cooldown up (SMSG_CAST_FAILED with SPELL_FAILED_NOT_READY);
     *  3. Blast Wave: (851..1003 + 171) x 1.2875 = 1315..1511, and its slow's holder on the Camel right after the cast
     *     lasts 3000 ms;
     *  4. and 5. Frostfire Bolt twice, each (782..996 + 1169) x 1.2875 = 2511..2787; the holder at each read, with the
     *     amount of its slow (effect 0, -40 in the data), as the record finds them;
     *  6. Pyroblast: (1238..1572 + 1849) x 1.2875 = 3974..4404; its hit makes Critical Mass 3 cast 22959 on the Camel;
     *  7. the Camel's health set to 30% of its maximum, where 22959 and the spell crit chance it gives attackers are read
     *     on the Camel: 5;
     *  8. Fire Blast, its cooldown taken off him by call first, at the Camel under 35%: Molten Fury 2 adds 8%:
     *     (955..1131 + 513) x 1.2875 x 1.08 = 2041..2285.
     * The amounts may lie one point outside their band, for the float product of the percent auras. The stage read
     * notes Improved Fire Blast's two modifiers on Fire Blast: crit +8 from 0, range +10 from 30.
     *
     * `passives`: `fireBlast=<a>,notReady=<result>,blastWave=<a>/<duration>,frostfire=<h1>+<h2>/eff<mask>/stack<n>/<slow>,
     * pyroblast=<a>,critMass=<22959 or none>/<crit taken>,moltenFury=<a>`; OK when every amount lies in its band, step 2
     * was refused NOT_READY, the slow lasted 3000, 22959 is on the Camel after step 6 and gives 5, and the holder and slow
     * of Frostfire Bolt are stated as found; BUG with the value in the detail otherwise.
     */
    class MageFireDamagePassives : public MageFirePassiveScenario
    {
    public:
        MageFireDamagePassives() : MageFirePassiveScenario("mage-fire-damage-passives", 985, "passives") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = FireTalents();
            talents.push_back(std::make_pair(10541u, 2u));
            return talents;
        }
        std::vector<uint32> Spells() const override { return { kFireBlast, kFrostfireBolt }; }

        std::vector<Step> Steps() const override
        {
            Step notReady = Cast(kFireBlast, false);
            notReady.refusal = true;
            Step cleared = Cast(kFireBlast, false);
            cleared.clearCooldown = true;
            return { Cast(kFireBlast, false), notReady, Cast(kBlastWave, false), Cast(kFrostfireBolt, false), Cast(kFrostfireBolt, false),
                     Cast(kPyroblast, false), TargetHealth(kUnderHealthPct, kHealthEveryMs), cleared };
        }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { kFireBlast, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { kFireBlast, SPELLMOD_RANGE, 30.0f } };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> own = {
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11113: 49 fields, fnv ac35ef1b
                { 11113, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1041395352 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 33 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -70 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
            const std::vector<SpellPrint> criticalMass = CriticalMassPrints();
            own.insert(own.end(), criticalMass.begin(), criticalMass.end());
            return PassivePrints(own);
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            if (!atRead && step.spell == kBlastWave)
            {
                seen.reads["duration"] = HolderDuration(target, kBlastWave);
                m_rec.Note("11113 on the Camel right after the cast: " + HolderOf(target, kBlastWave) + ", maximum duration " +
                           std::to_string(seen.reads["duration"]));
            }
            else if (atRead && step.spell == kFrostfireBolt)
            {
                SpellAuraHolder* holder = target->GetSpellAuraHolder(kFrostfireBolt);
                uint32 mask = 0;
                for (uint32 e = 0; holder && e < MAX_EFFECT_INDEX; ++e)
                {
                    mask |= holder->GetAuraByEffectIndex(SpellEffectIndex(e)) ? 1u << e : 0u;
                }
                Aura* slow = holder ? holder->GetAuraByEffectIndex(EFFECT_INDEX_0) : NULL;
                seen.reads["mask"] = mask;
                seen.reads["stack"] = holder ? holder->GetStackAmount() : 0;
                seen.reads["slow"] = slow ? slow->GetModifier()->m_amount : 0;
                m_rec.Note("44614 on the Camel: " + HolderOf(target, kFrostfireBolt) + "; its effect 0 (the slow) " +
                           (slow ? std::to_string(slow->GetModifier()->m_amount) : std::string("absent")));
            }
            else if (!atRead && step.healthPct)
            {
                seen.reads["critMass"] = target->HasAura(kCriticalMassDebuff) ? 1 : 0;
                seen.reads["critTaken"] = target->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_CRIT_CHANCE);
                m_rec.Note("22959 on the Camel: " + HolderOf(target, kCriticalMassDebuff) + "; the spell crit chance its attackers gain " +
                           std::to_string(seen.reads["critTaken"]));
            }
        }

        std::string Own(St const& st) const override
        {
            const int32 notReady = CastFailedResult(kNotReadyStep, kFireBlast);
            const int64 slowDuration = Kept(st, kBlastWaveStep, "duration");
            const int64 critMass = Kept(st, kHealthStep, "critMass");
            const int64 critTaken = Kept(st, kHealthStep, "critTaken");
            const int64 mask = Kept(st, kSecondBoltStep, "mask");
            const int64 stack = Kept(st, kSecondBoltStep, "stack");
            const int64 slow = Kept(st, kSecondBoltStep, "slow");

            char frostfire[96];
            snprintf(frostfire, sizeof(frostfire), "%s+%s/eff0x%llx/stack%lld/%lld", AmountText(st, kFirstBoltStep).c_str(),
                     AmountText(st, kSecondBoltStep).c_str(), (unsigned long long)(mask < 0 ? 0 : mask), (long long)stack, (long long)slow);
            const std::string value = "fireBlast=" + AmountText(st, kFireBlastStep) + ",notReady=" + CastResultName(notReady) +
                                      ",blastWave=" + AmountText(st, kBlastWaveStep) + "/" + std::to_string(slowDuration) +
                                      ",frostfire=" + frostfire + ",pyroblast=" + AmountText(st, kPyroblastStep) +
                                      ",critMass=" + (critMass == 1 ? U(kCriticalMassDebuff) : std::string("none")) + "/" +
                                      std::to_string(critTaken) + ",moltenFury=" + AmountText(st, kMoltenFuryStep);

            const bool ok = HitInBand(st, kFireBlastStep, 1890, 2116) && notReady == SPELL_FAILED_NOT_READY &&
                            !st.seen[kNotReadyStep].logRead && HitInBand(st, kBlastWaveStep, 1315, 1511) && slowDuration == 3000 &&
                            HitInBand(st, kFirstBoltStep, 2511, 2787) && HitInBand(st, kSecondBoltStep, 2511, 2787) &&
                            HitInBand(st, kPyroblastStep, 3974, 4404) && critMass == 1 && critTaken == 5 &&
                            HitInBand(st, kMoltenFuryStep, 2041, 2285);
            const std::string detail =
                "Fire Blast 1890..2116, Blast Wave 1315..1511 with its slow 3000 ms, Frostfire Bolt 2511..2787 (the holder after the second "
                "bolt and its slow as found: unglyphed in 4.3.4 the slow stays at one stack of -40), Pyroblast 3974..4404 with 22959 on "
                "the Camel after it (5% spell crit taken), Fire Blast under 35% 2041..2285; step 2's refusal " + CastResultName(notReady) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kUnderHealthPct = 30;
        static constexpr uint32 kHealthEveryMs = 2000;
        static constexpr uint32 kFireBlastStep = 0;
        static constexpr uint32 kNotReadyStep = 1;
        static constexpr uint32 kBlastWaveStep = 2;
        static constexpr uint32 kFirstBoltStep = 3;
        static constexpr uint32 kSecondBoltStep = 4;
        static constexpr uint32 kPyroblastStep = 5;
        static constexpr uint32 kHealthStep = 6;
        static constexpr uint32 kMoltenFuryStep = 7;
    };

    /**
     * S986 `mage-fire-cast-passives`: the Fire passives that change how a cast goes, on a Fire mage staged with
     * FireTalents() (25 points) and Glyph of Fireball (316) in a prime slot; Scorch (2948) and Fire Blast (2136)
     * learned. The stage read notes the glyph's modifier: Fireball's crit +5 from 0. Steps:
     *  1. Scorch: Improved Scorch 2 takes its cost to 0 (the cast's own cost right after it starts, Spell::GetPowerCost,
     *     which Spell::CalculatePowerCost gives with the caster's cost modifiers), and his mana at the read is his
     *     maximum;
     *  2. Scorch while his client's movement word says he moves forward: Firestarter (86914, aura 330 on Scorch) lets it
     *     go;
     *  3. Fireball while moving: refused with SPELL_FAILED_MOVING (Spell::CheckCast, aura 330's exception for Scorch
     *     alone);
     *  4. Fireball, 2.5 s, which the Camel's swing strikes 1.0 s in: Burning Soul 3 cuts the 500 ms pushback by 70% to
     *     150 (SMSG_SPELL_DELAYED);
     *  5. Dragon's Breath (31661): its confuse on the Camel has one proc charge;
     *  6. Fire Blast 1.5 s later: the confuse is on the Camel when the step runs and gone right after the hit;
     *  7.-16. ten swings of the Camel at him: Blazing Speed 2 casts 31643 on him at 10% of the swings that hit.
     *
     * `casts`: `fireballCrit=<the glyph's read>,scorchCost=<cost>,moving=<step 2's spell or none>/<step 3's>,
     * pushback=<the delays of step 4>,dragonsBreath=<gone|kept|absent>,blazingSpeed=<the swings after which 31643 was
     * cast>`; OK when the glyph's read is 5, the cost is 0 with his mana at its maximum, the moving Scorch went and the
     * moving Fireball was refused MOVING, the pushback is 150, the confuse is gone, and every 31643 follows a swing that
     * hit; BUG with the value in the detail otherwise.
     */
    class MageFireCastPassives : public MageFirePassiveScenario
    {
    public:
        MageFireCastPassives() : MageFirePassiveScenario("mage-fire-cast-passives", 986, "casts") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return FireTalents(); }
        std::vector<uint32> Spells() const override { return { kScorch, kFireBlast }; }
        std::vector<uint32> Glyphs() const override { return { 316 }; }
        uint32 Watched() const override { return kDragonsBreath; }
        bool WatchedOnTarget() const override { return true; }

        std::vector<ModifierRead> Modifiers() const override { return { { kFireball, SPELLMOD_CRITICAL_CHANCE, 0.0f } }; }

        std::vector<Step> Steps() const override
        {
            Step movingFireball = MovingCast(kFireball);
            movingFireball.refusal = true;
            std::vector<Step> steps = { Cast(kScorch, false), MovingCast(kScorch), movingFireball, CastStruckAt(kFireball, kSwingAtMs),
                                        AuraCast(kDragonsBreath, kBreathEveryMs), Cast(kFireBlast, false) };
            steps.insert(steps.end(), kSwings, Swing());
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return PassivePrints({
                // 2948: 26 fields, fnv a00256b8
                { 2948, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 52 }, { "eff0.EffectBonusCoefficient", 1057165935 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 13 }, { "eff0.EffectRealPointsPerLevel", 1063675494 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31661: 49 fields, fnv 00498b1e
                { 31661, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 369 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 61 }, { "eff0.EffectRadiusIndex_0", 32 }, { "eff0.EffectRealPointsPerLevel", 1069547520 }, { "eff0.ImplicitTarget_0", 104 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 2 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 104 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            if (!atRead && step.spell == kScorch && !step.moving)
            {
                Spell const* cast = p->GetCurrentSpell(CURRENT_GENERIC_SPELL);
                seen.reads["powerCost"] = cast && cast->m_spellInfo->ID == kScorch ? int64(cast->GetPowerCost()) : -1;
                m_rec.Note("the Scorch's own cost right after it starts: " + std::to_string(seen.reads["powerCost"]));
            }
            else if (atRead && step.spell == kScorch && !step.moving)
            {
                seen.reads["mana"] = p->GetPower(POWER_MANA);
                seen.reads["manaMax"] = p->GetMaxPower(POWER_MANA);
            }
            else if (!atRead && step.spell == kFireBlast)
            {
                seen.reads["confuseAfter"] = target->HasAura(kDragonsBreath) ? 1 : 0;
                m_rec.Note("31661 on the Camel right after the hit: " + HolderOf(target, kDragonsBreath));
            }
        }

        std::string Own(St const& st) const override
        {
            Seen const& scorch = st.seen[kScorchStep];
            const int64 scorchCost = Kept(st, kScorchStep, "powerCost");
            const bool manaKept = Kept(st, kScorchStep, "mana") >= 0 && Kept(st, kScorchStep, "mana") == Kept(st, kScorchStep, "manaMax");
            const std::string movingScorch = st.seen[kMovingScorchStep].logRead ? U(kScorch) : std::string("none");
            const std::string movingFireball = st.seen[kMovingFireballStep].logRead ? U(kFireball) : std::string("none");
            const int32 movingRefusal = CastFailedResult(kMovingFireballStep, kFireball);

            const std::vector<uint32> delays = CastDelays(kStruckStep);
            std::string pushback;
            for (size_t i = 0; i < delays.size(); ++i)
            {
                pushback += (i ? "+" : "") + U(delays[i]);
            }
            Seen const& struck = st.seen[kStruckStep];

            Seen const& fireBlast = st.seen[kFireBlastStep];
            const bool before = !fireBlast.watchedBefore.empty() && fireBlast.watchedBefore != "none";
            const int64 after = Kept(st, kFireBlastStep, "confuseAfter");
            const std::string breath = !before ? "absent" : after == 0 ? "gone" : "kept";

            bool speedOk = true;
            for (uint32 k = kFirstSwingStep; k < st.steps.size(); ++k)
            {
                speedOk = speedOk && (!Triggered(st.seen[k], kBlazingSpeed) || st.seen[k].healthAfter < st.seen[k].healthBefore);
            }
            std::string speed = StepsWhere(st, [&st](uint32 k) { return k >= kFirstSwingStep && Triggered(st.seen[k], kBlazingSpeed); });
            std::replace(speed.begin(), speed.end(), ',', '+');
            const std::string hits = StepsWhere(st, [&st](uint32 k)
            {
                return k >= kFirstSwingStep && st.seen[k].healthAfter < st.seen[k].healthBefore;
            });

            char glyph[32];
            snprintf(glyph, sizeof(glyph), "%.0f", st.modifiers.empty() ? -1.0f : st.modifiers[0]);
            const std::string value = std::string("fireballCrit=") + glyph + ",scorchCost=" + std::to_string(scorchCost) + ",moving=" +
                                      movingScorch + "/" + movingFireball + ",pushback=" + (pushback.empty() ? std::string("none") : pushback) +
                                      ",dragonsBreath=" + breath + ",blazingSpeed=" + speed;
            const bool ok = st.modifiers.size() == 1 && std::fabs(st.modifiers[0] - 5.0f) < 0.01f && scorchCost == 0 && manaKept &&
                            movingScorch == U(kScorch) && movingFireball == "none" && movingRefusal == SPELL_FAILED_MOVING &&
                            pushback == "150" && breath == "gone" && speedOk;
            const std::string detail =
                "Scorch's cost " + std::to_string(scorchCost) + " (" + U(scorch.cost) + " without the cost modifiers), his mana at its read " +
                std::to_string(Kept(st, kScorchStep, "mana")) + "/" +
                std::to_string(Kept(st, kScorchStep, "manaMax")) + "; the moving Fireball's refusal " + CastResultName(movingRefusal) +
                "; the swing during the Fireball took his health " + U(struck.swingHealthBefore) + "->" + U(struck.swingHealthAfter) +
                ", the Fireball " + AmountText(st, kStruckStep) + "; 31661 on the Camel before the Fire Blast: " +
                (fireBlast.watchedBefore.empty() ? std::string("none") : fireBlast.watchedBefore) + "; 31643 after swings " + speed +
                ", the swings that hit " + hits + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kScorch = 2948;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kDragonsBreath = 31661;
        static constexpr uint32 kBlazingSpeed = 31643;
        static constexpr uint32 kSwingAtMs = 1000;
        static constexpr uint32 kBreathEveryMs = 1500;
        static constexpr uint32 kSwings = 10;
        static constexpr uint32 kScorchStep = 0;
        static constexpr uint32 kMovingScorchStep = 1;
        static constexpr uint32 kMovingFireballStep = 2;
        static constexpr uint32 kStruckStep = 3;
        static constexpr uint32 kFireBlastStep = 5;
        static constexpr uint32 kFirstSwingStep = 6;
    };

    /**
     * S987 `mage-fire-glyphs`: six Fire glyphs on a Fire mage staged with FireTalents(), Critical Mass 3, Pyromaniac 2
     * and Living Bomb (31 points, Living Bomb's tier asks 30): Glyph of Pyroblast (320), Glyph of Molten Armor (328) and
     * Glyph of Living Bomb (928) in the three prime slots, Glyph of Blast Wave (611) and Glyph of Dragon's Breath (331)
     * major, Glyph of Armors (923) minor; Molten Armor (30482) learned. The stage read notes each glyph's modifier through
     * his modifier manager: Pyroblast's and Pyroblast!'s crit +5 from 0; Molten Armor's effect 3 +2 from 3; Living Bomb's
     * periodic damage and its explosion's damage from 100: +3 from the glyph and +15 from Critical Mass 3 (12873's
     * effects 2 and 1), 118; Blast Wave's duration +1000 from 3000; Dragon's Breath's cooldown -3000 from 20000; Molten
     * Armor's duration +1800000 from 1800000. Steps:
     *  1. Molten Armor cast at himself: its holder lasts 3600000 ms and his sheet's fire spell crit rises by 5 (3 and the
     *     glyph's 2);
     *  2. Blast Wave, which must not crit: its slow's holder on the Camel right after the cast lasts 4000 ms;
     *  3. Living Bomb, left to run out: each tick (234 + int(1197 x 0.258) = 308) x 1.2875 x 1.18 = 823.
     *
     * `glyphs`: `mods=<the eight values>,moltenArmor=<its maximum duration>/<the crit rise>,blastWave=<the slow's maximum
     * duration>,bomb=<the ticks of 44457>`; OK when the values are 5/5/5/118/118/4000/17000/3600000, Molten Armor lasts
     * 3600000 and raises his crit by 5, the slow lasts 4000 and every tick is 823 (one point either side); BUG with the
     * value in the detail otherwise.
     */
    class MageFireGlyphs : public MageFirePassiveScenario
    {
    public:
        MageFireGlyphs() : MageFirePassiveScenario("mage-fire-glyphs", 987, "glyphs") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = FireTalents();
            talents.push_back(std::make_pair(10541u, 2u));
            talents.push_back(std::make_pair(10559u, 1u));
            talents.push_back(std::make_pair(10577u, 0u));
            return talents;
        }
        std::vector<uint32> Spells() const override { return { kMoltenArmor }; }
        std::vector<uint32> Glyphs() const override { return { 320, 328, 928, 611, 331, 923 }; }
        bool TakesDamage() const override { return true; }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kMoltenArmor), Cast(kBlastWave, false), AuraCast(kLivingBomb, kBombEveryMs) };
        }

        std::vector<ModifierRead> Modifiers() const override
        {
            return { { 11366, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { 92315, SPELLMOD_CRITICAL_CHANCE, 0.0f }, { kMoltenArmor, SPELLMOD_EFFECT3, 3.0f },
                     { kLivingBomb, SPELLMOD_DOT, 100.0f }, { 44461, SPELLMOD_DAMAGE, 100.0f }, { kBlastWave, SPELLMOD_DURATION, 3000.0f },
                     { 31661, SPELLMOD_COOLDOWN, 20000.0f }, { kMoltenArmor, SPELLMOD_DURATION, 1800000.0f } };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> own = {
                // 30482: 72 fields, fnv 920fa0aa
                { 30482, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 34913 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 197 }, { "eff1.EffectBasePoints", -5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 57 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1792 }, { "eff2.EffectMiscValue_1", 4 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11113: 49 fields, fnv ac35ef1b
                { 11113, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1041395352 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 33 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -70 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            const std::vector<SpellPrint> criticalMass = CriticalMassPrints();
            own.insert(own.end(), criticalMass.begin(), criticalMass.end());
            return PassivePrints(own);
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Step const& step = st.steps[k];
            Seen& seen = st.seen[k];
            if (atRead && step.self && step.spell == kMoltenArmor)
            {
                const float crit = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE);
                seen.reads["duration"] = HolderDuration(p, kMoltenArmor);
                seen.reads["critRise"] = int64(std::lround((crit - st.stagedFireCrit) * 100.0f));
                char buf[160];
                snprintf(buf, sizeof(buf), "30482 on him: maximum duration %lld; spell crit %.2f%% (fire), %.2f at the stage read",
                         (long long)seen.reads["duration"], crit, st.stagedFireCrit);
                m_rec.Note(buf);
            }
            else if (!atRead && step.spell == kBlastWave)
            {
                seen.reads["duration"] = HolderDuration(target, kBlastWave);
                m_rec.Note("11113 on the Camel right after the cast: " + HolderOf(target, kBlastWave) + ", maximum duration " +
                           std::to_string(seen.reads["duration"]));
            }
        }

        std::string Own(St const& st) const override
        {
            const float wanted[] = { 5.0f, 5.0f, 5.0f, 118.0f, 118.0f, 4000.0f, 17000.0f, 3600000.0f };
            const size_t count = sizeof(wanted) / sizeof(wanted[0]);
            std::string mods;
            bool ok = st.modifiers.size() == count;
            for (size_t i = 0; i < st.modifiers.size(); ++i)
            {
                char buf[32];
                snprintf(buf, sizeof(buf), "%.0f", st.modifiers[i]);
                mods += (i ? "/" : "") + std::string(buf);
                ok = ok && i < count && std::fabs(st.modifiers[i] - wanted[i]) < 0.01f;
            }

            const int64 armorDuration = Kept(st, kArmorStep, "duration");
            const int64 critRise = Kept(st, kArmorStep, "critRise");
            const int64 slowDuration = Kept(st, kBlastWaveStep, "duration");
            const std::vector<uint32> ticks = TickAmounts(kBombStep, kLivingBomb);
            std::string tickText;
            bool ticksOk = !ticks.empty();
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                tickText += (i ? "+" : "") + U(ticks[i]);
                ticksOk = ticksOk && InBand(ticks[i], kBombTick, kBombTick);
            }

            char rise[32];
            snprintf(rise, sizeof(rise), "%+.2f", critRise / 100.0);
            const std::string value = "mods=" + (mods.empty() ? std::string("none") : mods) + ",moltenArmor=" + std::to_string(armorDuration) +
                                      "/" + rise + ",blastWave=" + std::to_string(slowDuration) + ",bomb=" +
                                      (tickText.empty() ? std::string("none") : tickText);
            ok = ok && armorDuration == 3600000 && critRise >= 499 && critRise <= 501 && slowDuration == 4000 && ticksOk;
            const std::string detail = "the reads 11366 and 92315 crit, 30482 effect 3, 44457 periodic, 44461 damage, 11113 duration, 31661 "
                                       "cooldown, 30482 duration, wanted 5,5,5,118,118,4000,17000,3600000; Molten Armor 3600000 ms and +5.00 "
                                       "spell crit; Blast Wave's slow 4000 ms; each Living Bomb tick " + U(kBombTick) + ")";
            return ok ? value + "(OK: " + detail : "BUG(" + value + ": " + detail;
        }

    private:
        static constexpr uint32 kMoltenArmor = 30482;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kBombEveryMs = 14000;
        static constexpr uint32 kBombTick = 823;
        static constexpr uint32 kArmorStep = 0;
        static constexpr uint32 kBlastWaveStep = 1;
        static constexpr uint32 kBombStep = 2;
    };

    /**
     * S983 `mage-fire-molten-fury-tick`: Molten Fury rank 3 (86880, class script 12368, +12 against a target under 35%
     * health) on a staged Fire mage with kSpellPower spell power, 1197 with his intellect's, and no mastery. Staged:
     * Burning Soul 3, Improved Fire Blast 2 and Master of Elements 2; Fire Power 3 and Blazing Speed 2; Blast Wave,
     * Improved Scorch 2 and Cauterize 2; Molten Shields, Combustion and Firestarter (the 20 points the fifth tier
     * asks), then Molten Fury 3; Fire Blast (2136) learned. Burning Soul names the Fire tree, which teaches Fire
     * Specialization (+25% fire damage) and Pyroblast; with Fire Power 3 every fire hit and tick is multiplied by
     * 1.25 x 1.03 = 1.2875. Before each cast he takes Precision (13843), so no cast misses, and the crit aura at -100,
     * so neither cast nor tick crits. Steps:
     *  1. Pyroblast (11366) at the Camel at full health: it lands about 3.9 s after the cast starts, and its DoT ticks
     *     every 3000 ms for 12 s, (164 + int(1197 x 0.18)) x 1.2875 = 487 a tick;
     *  2. kHealthAfterMs after the cast, between the first tick and the second, the Camel's health set to kUnderPct
     *     percent of its maximum; its next update sets its under-35% state;
     *  3. kTicksEveryMs later, after the DoT's last tick, Fire Blast at the Camel under 35%.
     * Molten Fury is read at each tick of a periodic damage effect and at a direct hit: the first tick, dealt above
     * 35%, is the plain tick; the three after the health step are int(plain x 1.12); Fire Blast's hit is (955..1131 +
     * int(1197 x 0.429)) x 1.2875 x 1.12 = 2116..2370.
     *
     * `moltenFury`: `ticks=<the 11366 ticks in the steps' windows, in order>,direct=<Fire Blast's hit>`; OK when there
     * are four ticks, the first is kPlainTick, the three after it are each the first times (100 + the amount of 86880's
     * aura on him) / 100 within one point, the Camel was not under 35% at the first read and was at the second, and the
     * hit lies in the band of Fire Blast's base, his fire spell power's share and the two multipliers, one point either
     * side allowed for the float product of the multipliers.
     */
    class MageFireMoltenFuryTick : public MageProcScenario
    {
    public:
        MageFireMoltenFuryTick() : MageProcScenario("mage-fire-molten-fury-tick", 983, "moltenFury") {}

    protected:
        MageFireMoltenFuryTick(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 }, { 11433, 1 },
                     { 10543, 0 }, { 10561, 0 }, { 11431, 0 }, { 10563, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFireBlast }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kPyroblast; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, false);
            pyroblast.everyMs = kHealthAfterMs;
            return { pyroblast, TargetHealth(kUnderPct, kTicksEveryMs), Cast(kFireBlast, false) };
        }

        /// At each read: whether the Camel's under-35% state (AURA_STATE_HEALTHLESS_35_PERCENT) is set, kept as
        /// `under35`.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            const bool under = target->HasAuraState(AURA_STATE_HEALTHLESS_35_PERCENT);
            st.seen[k].reads["under35"] = under ? 1 : 0;
            m_rec.Note("the Camel's health " + U(target->GetHealth()) + " of " + U(target->GetMaxHealth()) +
                       (under ? ", under 35%" : ", not under 35%"));
        }

        /// What ReadOwn kept of step k: 1 when the Camel was under 35% at its read, 0 when not, -1 when unread.
        static int64 Under35(St const& st, uint32 k)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find("under35");
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        static char const* Under35Text(int64 under)
        {
            return under == 1 ? "yes" : under == 0 ? "no" : "unread";
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            std::vector<uint32> ticks;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::vector<uint32> stepTicks = TickAmounts(k, kPyroblast);
                ticks.insert(ticks.end(), stepTicks.begin(), stepTicks.end());
            }

            int32 amount = 0;
            int32 spellPower = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                if (Aura const* moltenFury = p->GetAura(kMoltenFury3, EFFECT_INDEX_0))
                {
                    amount = moltenFury->GetModifier()->m_amount;
                }
                spellPower = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
            }
            SpellEntry const* fireBlast = sSpellStore.LookupEntry(kFireBlast);
            SpellEffectEntry const* fireBlastHit = fireBlast ? fireBlast->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const float coefficient = fireBlastHit ? fireBlastHit->EffectBonusCoefficient : 0.0f;
            const int32 share = int32(spellPower * coefficient);
            const float lowHealth = (100.0f + amount) / 100.0f;
            const uint32 low = uint32(float(kFireBlastLow + share) * kDoneMultiplier * lowHealth);
            const uint32 high = uint32(float(kFireBlastHigh + share) * kDoneMultiplier * lowHealth);
            const uint32 plainLow = uint32(float(kFireBlastLow + share) * kDoneMultiplier);
            const uint32 plainHigh = uint32(float(kFireBlastHigh + share) * kDoneMultiplier);

            Seen const& pyroblast = st.seen[0];
            Seen const& health = st.seen[1];
            Seen const& direct = st.seen[2];
            const uint32 first = ticks.empty() ? 0 : ticks[0];
            const uint32 raised = uint32(first * lowHealth);
            const int64 underAtFirst = Under35(st, 0);
            const int64 underAtHealth = Under35(st, 1);
            bool ok = st.steps.size() == 3 && amount > 0 && ticks.size() == kTicks && first == kPlainTick && underAtFirst == 0 &&
                      underAtHealth == 1 && direct.logRead && !direct.crit && InBand(direct.amount, low, high);
            std::string tickText;
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                tickText += (i ? "," : "") + U(ticks[i]);
                if (i > 0)
                {
                    ok = ok && InBand(ticks[i], raised, raised);
                }
            }

            const std::string value = "ticks=" + (tickText.empty() ? std::string("none") : tickText) + ",direct=" +
                                      (direct.logRead ? U(direct.amount) : std::string("none"));
            const std::string directText = direct.logRead ? U(direct.amount) + (direct.crit ? " crit" : "") : std::string("none");
            const std::string pyroblastText = pyroblast.logRead ? U(pyroblast.amount) + (pyroblast.crit ? " crit" : "") : std::string("none");
            char buf[768];
            snprintf(buf, sizeof(buf),
                     "%u ticks of 11366 (%u wanted), the first %u above 35%% (the plain tick %u wanted); each tick after the health "
                     "step int(%u x %.2f) = %u wanted, one point either side; the Camel under 35%% at the first read %s, at the "
                     "second %s (its health %u -> %u); 86880's amount on him %d; fire spell power %d, Fire Blast's share int(%d x "
                     "%.3f) = %d; Fire Blast's hit %s, the band (%d..%d + %d) x %.4f x %.2f = %u..%u (without Molten Fury %u..%u); "
                     "Pyroblast's hit %s",
                     uint32(ticks.size()), uint32(kTicks), first, kPlainTick, first, lowHealth, raised,
                     Under35Text(underAtFirst), Under35Text(underAtHealth), health.healthBefore,
                     health.healthAfter, amount, spellPower, spellPower, coefficient, share, directText.c_str(), kFireBlastLow,
                     kFireBlastHigh, share, kDoneMultiplier, lowHealth, low, high, plainLow, plainHigh, pyroblastText.c_str());
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kMoltenFury3 = 86880;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kHealthAfterMs = 7800;
        static constexpr uint32 kTicksEveryMs = 9000;
        static constexpr uint32 kUnderPct = 30;
        static constexpr size_t kTicks = 4;
        static constexpr uint32 kPlainTick = 487;
        static constexpr int32 kFireBlastLow = 955;
        static constexpr int32 kFireBlastHigh = 1131;
        static constexpr float kDoneMultiplier = 1.25f * 1.03f;
    };

    /**
     * S978 `mage-fire-combustion-molten-fury`: 983's staging (Molten Fury rank 3, +12 against a target under 35%
     * health, Combustion, Fire Specialization and Fire Power 3, no mastery, no crit), and Combustion's DoT (83853) as
     * it carries the Molten Fury of the Pyroblast DoT it combines. Steps:
     *  1. Pyroblast (11366) at the Camel at full health: its DoT holds a tick of p = 487;
     *  2. kCombustionAfterMs later, Combustion at the Camel above 35%: its DoT ticks int(p x 1000 / 3000) = 162;
     *  3. the Camel's health set to kUnderPct percent of its maximum;
     *  4. Combustion again, its cooldown taken off him by call first, with the Camel under 35%: the DoT, which replaces
     *     step 2's, ticks int(p x 1000 / 3000 x 1.12) = 181 for its whole duration. One plain 162 tick of the replaced
     *     DoT lands after step 3, with the Camel under 35%: 83853 takes no Molten Fury of its own, so the 12% comes
     *     only from the share of the Pyroblast DoT.
     *
     * `combustionMoltenFury`: `above=<tick>x<count>,below=<tick>x<count>`: the first and the count of the 83853 ticks
     * dealt before step 4 and from it on. OK when the Pyroblast DoT's tick is the plain kPlainTick, the Camel was not
     * under 35% at step 2's read and was at step 4's, every tick before step 4 is the plain share, at least
     * kMinAbove of them, and every tick from step 4 on is the share times (100 + the amount of 86880's aura on him)
     * / 100, exactly kBelowTicks + 1 of them (Combustion's duration over its period, and one more when the DoT
     * replaces a running one).
     */
    class MageFireCombustionMoltenFury : public MageFireMoltenFuryTick
    {
    public:
        MageFireCombustionMoltenFury() : MageFireMoltenFuryTick("mage-fire-combustion-molten-fury", 978, "combustionMoltenFury") {}

    protected:
        uint32 Watched() const override { return kCombustionDot; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, false);
            pyroblast.everyMs = kCombustionAfterMs;
            Step above = Cast(kCombustion, false);
            above.everyMs = kHealthAfterMs;
            Step below = Cast(kCombustion, false);
            below.clearCooldown = true;
            below.everyMs = kLastStepMs;
            return { pyroblast, above, TargetHealth(kUnderPct, kRecastAfterMs), below };
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFireMoltenFuryTick::Prints();
            const std::vector<SpellPrint> own = {
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            prints.insert(prints.end(), own.begin(), own.end());
            return prints;
        }

        std::string Own(St const& st) const override
        {
            std::vector<uint32> plain;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::vector<uint32> stepTicks = TickAmounts(k, kPyroblast);
                plain.insert(plain.end(), stepTicks.begin(), stepTicks.end());
            }
            std::vector<uint32> above;
            std::vector<uint32> below;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const std::vector<uint32> stepTicks = TickAmounts(k, kCombustionDot);
                std::vector<uint32>& into = k < kBelowStep ? above : below;
                into.insert(into.end(), stepTicks.begin(), stepTicks.end());
            }

            int32 amount = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                if (Aura const* moltenFury = p->GetAura(kMoltenFury3, EFFECT_INDEX_0))
                {
                    amount = moltenFury->GetModifier()->m_amount;
                }
            }
            const uint32 first = plain.empty() ? 0 : plain[0];
            const float lowHealth = (100.0f + amount) / 100.0f;
            const uint32 wantAbove = uint32(double(first) * 1000.0 / double(kPyroblastPeriod));
            const uint32 wantBelow = uint32(double(first) * 1000.0 / double(kPyroblastPeriod) * double(lowHealth));
            const int64 underAbove = Under35(st, 1);
            const int64 underBelow = Under35(st, kBelowStep);

            bool ok = st.steps.size() == 4 && amount > 0 && first == kPlainTick && underAbove == 0 && underBelow == 1 &&
                      above.size() >= kMinAbove && below.size() == kBelowTicks + 1;
            for (size_t i = 0; i < above.size(); ++i)
            {
                ok = ok && above[i] == wantAbove;
            }
            for (size_t i = 0; i < below.size(); ++i)
            {
                ok = ok && below[i] == wantBelow;
            }

            const std::string value = "above=" + (above.empty() ? std::string("none") : U(above[0])) + "x" + U(above.size()) +
                                      ",below=" + (below.empty() ? std::string("none") : U(below[0])) + "x" + U(below.size());
            char buf[512];
            snprintf(buf, sizeof(buf),
                     "the Pyroblast DoT's tick %u (the plain tick %u wanted); 83853 ticks before step 4 %u, each int(%u x 1000 / %u) = %u "
                     "wanted (at least %u); from step 4 %u, exactly %u wanted, each int(%u x 1000 / %u x %.2f) = %u; the Camel under 35%% at "
                     "step 2's read %s, at step 4's %s; 86880's amount on him %d",
                     first, kPlainTick, uint32(above.size()), first, kPyroblastPeriod, wantAbove, uint32(kMinAbove), uint32(below.size()),
                     uint32(kBelowTicks + 1), first, kPyroblastPeriod, lowHealth, wantBelow, Under35Text(underAbove), Under35Text(underBelow),
                     amount);
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kCombustion = 11129;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kMoltenFury3 = 86880;
        static constexpr uint32 kPyroblastPeriod = 3000;
        static constexpr uint32 kCombustionAfterMs = 4300;
        static constexpr uint32 kHealthAfterMs = 4000;
        static constexpr uint32 kRecastAfterMs = 1000;
        static constexpr uint32 kLastStepMs = 12000;
        static constexpr uint32 kUnderPct = 30;
        static constexpr uint32 kBelowStep = 3;
        static constexpr uint32 kPlainTick = 487;
        static constexpr size_t kMinAbove = 3;
        static constexpr size_t kBelowTicks = 10;
    };

    /**
     * What the two Fire proof scenarios share (988, 989): a Fire mage staged with Burning Soul 3, Improved Fire Blast 2
     * and Fire Power 3 (the eight points of the first two tiers; Burning Soul names the Fire tree, which teaches Fire
     * Specialization, +25% fire damage, and Pyroblast 11366) and kSpellPower spell power, 1197 with his intellect's.
     * Fire Specialization and Fire Power 3 multiply a fire spell's done damage by 1.25 x 1.03 = 1.2875. No spell of his
     * crits: each cast step puts Increased Critical Spell on him at -100 first, and it stays on him to the end.
     */
    class MageFireProofScenario : public MageProcScenario
    {
    public:
        MageFireProofScenario(char const* name, int order, char const* ownCategory) : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kPrecision = 13843;

        std::vector<std::pair<uint32, uint32> > Talents() const override { return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 } }; }
        int32 SpellPower() const override { return kSpellPower; }
        int32 NonCritAuraPoints() const override { return -100; }

        /// The prints both scenarios need: the holders on him that can proc and what they cast, Precision and the crit
        /// aura; `own` are the scenario's spells.
        static std::vector<SpellPrint> ProofPrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        /// A read kept by ReadOwn for step k; -1 when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

    private:
        static constexpr int32 kSpellPower = 1000;
    };

    /**
     * S988 `mage-fire-molten-armor`: Molten Armor (30482) on a Fire mage staged as MageFireProofScenario stages him, and
     * its damage on the melee hits he takes. Its effect 0 procs on a hit he takes (proc flags 0x222a8, chance 100, no
     * charges) and casts 34913 at the attacker: school damage of 190 at 85 (SpellScaling 38: 0.203 of a mage's spell
     * scaling at 85), bonus coefficient 0, so his spell power adds nothing and his percent auras make 190 x 1.2875 =
     * 244.6, 244 once truncated. A swing he dodges, or one that misses him, casts nothing: without a spell_proc_event row
     * the proc asks for a hit (SpellMgr::IsSpellProcEventCanTriggeredBy). Its effect 1 takes 5 off the crit chance of
     * his attackers (aura 197), its effect 2 adds 3 to his spell crit (aura 57). Steps:
     *  1. Molten Armor cast through the CAST_SPELL handler's calls, which put Precision (13843, +6 spell hit, so no
     *     34913 misses the level-85 Camel) and the crit aura at -100 on him first; at the read his fire spell crit
     *     stands 3 above the staged one with the crit aura's -100 counted, and his attackers' crit chance is 5 lower;
     *  2.-6. five swings of the Camel at him.
     *
     * `moltenArmor`: `procs=<the 34913 casts>/<the swings that hit>,amount=<their amounts, one when all are equal>,
     * holder=<kept, or changed>,crit=<his crit rise>/<his attackers' crit change>`; OK when each swing that hit logged
     * one 34913 of 244 that did not crit and took 244 off the Camel's health, each other swing logged none and took
     * nothing, at least one swing hit, the holder on him read `eff 0x7 charges 0 stack 1` at every read, his crit rose
     * by 3 and his attackers' fell by 5.
     */
    class MageFireMoltenArmor : public MageFireProofScenario
    {
    public:
        MageFireMoltenArmor() : MageFireProofScenario("mage-fire-molten-armor", 988, "moltenArmor") {}

    protected:
        std::vector<uint32> Spells() const override { return { kMoltenArmor }; }
        bool TakesDamage() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 Watched() const override { return kMoltenArmor; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { AuraCast(kMoltenArmor, 0) };
            steps.insert(steps.end(), kSwings, Swing());
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 30482: 72 fields, fnv 920fa0aa
                { 30482, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 34913 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 197 }, { "eff1.EffectBasePoints", -5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 57 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1792 }, { "eff2.EffectMiscValue_1", 4 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (!atRead)
            {
                seen.reads["camel"] = target->GetHealth();
                if (st.steps[k].swing)
                {
                    m_rec.Note("the Camel's health right after the swing " + U(target->GetHealth()));
                }
            }
            else if (st.steps[k].spell == kMoltenArmor)
            {
                const float crit = p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE);
                const float rise = crit - st.stagedFireCrit - float(NonCritAuraPoints());
                seen.reads["critRise"] = int64(std::lround(rise * 100.0f));
                seen.reads["critTaken"] = p->GetTotalAuraModifier(SPELL_AURA_MOD_ATTACKER_SPELL_AND_WEAPON_CRIT_CHANCE);
                char buf[256];
                snprintf(buf, sizeof(buf),
                         "30482 on him: maximum duration %d; spell crit %.2f%% (fire), %.2f at the stage read and %d from the crit aura since: "
                         "%+.2f from Molten Armor; his attackers' crit chance changed by %lld",
                         HolderDuration(p, kMoltenArmor), crit, st.stagedFireCrit, NonCritAuraPoints(), rise,
                         (long long)seen.reads["critTaken"]);
                m_rec.Note(buf);
            }
        }

        std::string Own(St const& st) const override
        {
            uint32 hits = 0;
            size_t procs = 0;
            bool ok = true;
            std::set<std::string> distinct;
            std::string amounts, swings;
            for (uint32 k = 1; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const bool hit = seen.read && seen.healthAfter < seen.healthBefore;
                const std::vector<std::string> lines = HitAmounts(k, kMoltenArmorHit);
                const int64 before = Kept(st, k - 1, "camel");
                const int64 after = Kept(st, k, "camel");
                const int64 loss = before >= 0 && after >= 0 ? before - after : -1;
                std::string text;
                for (size_t i = 0; i < lines.size(); ++i)
                {
                    text += (i ? "," : "") + lines[i];
                    amounts += (amounts.empty() ? "" : ",") + lines[i];
                    distinct.insert(lines[i]);
                }
                hits += hit ? 1 : 0;
                procs += lines.size();
                ok = ok && seen.read &&
                     (hit ? lines.size() == 1 && lines[0] == U(kAmount) && loss == int64(kAmount) : lines.empty() && loss == 0);
                swings += "; " + U(k + 1) + (hit ? " hit" : " not hit") + ": 34913 [" + text + "], the Camel's health -" +
                          std::to_string(loss);
            }
            bool holderKept = true;
            std::string holders;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                holderKept = holderKept && st.seen[k].holder == kHolder;
                holders += (k ? "," : "") + (st.seen[k].holder.empty() ? std::string("unread") : st.seen[k].holder);
            }
            const int64 critRise = Kept(st, kArmorStep, "critRise");
            const int64 critTaken = Kept(st, kArmorStep, "critTaken");
            char crit[48];
            snprintf(crit, sizeof(crit), "%+.2f/%lld", critRise / 100.0, (long long)critTaken);
            const std::string amount = amounts.empty() ? std::string("none") : distinct.size() == 1 ? *distinct.begin() : amounts;
            const std::string value = "procs=" + std::to_string(procs) + "/" + U(hits) + ",amount=" + amount + ",holder=" +
                                      (holderKept ? "kept" : "changed") + ",crit=" + crit;
            ok = ok && hits > 0 && holderKept && critRise >= 299 && critRise <= 301 && critTaken == -5;
            const std::string detail = "each swing that hits him casts 34913 once for 190 x 1.2875 = " + U(kAmount) +
                                       " at the Camel, no other swing casts it" + swings + "; the holder at each read " + holders +
                                       ", wanted " + kHolder + "; his crit +3 and his attackers' -5 wanted)";
            return value + (ok ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kMoltenArmor = 30482;
        static constexpr uint32 kMoltenArmorHit = 34913;
        static constexpr uint32 kAmount = 244;
        static constexpr uint32 kSwings = 5;
        static constexpr uint32 kArmorStep = 0;
        static constexpr char const* kHolder = "eff 0x7 charges 0 stack 1";
    };

    /**
     * S948 `mage-fire-molten-armor-procs`: Molten Armor's hit (34913) starts procs, never ignites, never starts another
     * Molten Armor and never breaks Polymorph, Blind or Frost Nova. A Fire mage staged as in 968 (Burning Soul 3,
     * Improved Fire Blast 2, Ignite 3, Impact 2; kSpellPower spell power), Polymorph (118) learned, Precision on him
     * before each cast and Increased Critical Spell on him at kArmorCritAuraPoints from the first step to the end, so
     * every 34913 crits. The Camel holds Molten Armor (30482) too, put on it in the first step, and its -5 to the crit
     * chance of a spell cast at it would leave 99.23% at +100, where his sheet crit with his Molten Armor reads
     * 104.23%; at kArmorCritAuraPoints it leaves 109.23%. Steps: Molten Armor cast at himself; kSwings swings of the
     * Camel at him, each hit of which casts 34913 at the Camel; Polymorph cast at the Camel; then three phases in which
     * the Camel holds Polymorph, Blind (2094) and Frost Nova (122), and in each step of a phase he casts 34913 at it by
     * call as the armor's proc does (triggered, naming the holder of 30482), each read kSwingReadMs after: kStrikes,
     * kBlindStrikes and kNovaStrikes steps. Blind and Frost Nova are put on the Camel as the GM aura command puts an
     * aura, ready as a cast leaves it and for kPhaseAuraMs, the aura of the phase before taken off it.
     *
     * `armorProcs`: `crits=<34913 hits that crit>/<34913 hits>,ignite=<steps with 12654 on the Camel or cast>,
     * impact=<swings after which 64343 was on him>,answered=<34913 hits the Camel dealt>,
     * poly=<the Camel's 118 after the cast, then after each strike, 1 held>,
     * blind=<the Camel's 2094 after each strike, 1 held>,nova=<strikes after which its 122 was held>/<strikes>`.
     * OK when every 34913 hit crit and each swing that hit logged one, nothing of Ignite (12654) was ever on the Camel
     * or cast, Impact's 64343 was on him after at least one swing, the Camel dealt no 34913, Polymorph landed and each
     * strike logged one 34913 and left the Camel's aura of its phase on.
     */
    class MageFireMoltenArmorProcs : public MageFireProofScenario
    {
    public:
        MageFireMoltenArmorProcs() : MageFireProofScenario("mage-fire-molten-armor-procs", 948, "armorProcs") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 10537, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kMoltenArmor, kPolymorph }; }
        bool TakesDamage() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 Watched() const override { return kPolymorph; }
        bool WatchedOnTarget() const override { return true; }
        int32 NonCritAuraPoints() const override { return kArmorCritAuraPoints; }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps = { AuraCast(kMoltenArmor, 0) };
            steps.insert(steps.end(), kSwings, Swing());
            steps.push_back(AuraCast(kPolymorph, 0));
            steps.insert(steps.end(), kStrikes + kBlindStrikes + kNovaStrikes, Wait(kSwingEveryMs));
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 30482: 72 fields, fnv 920fa0aa
                { 30482, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 34913 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 197 }, { "eff1.EffectBasePoints", -5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 57 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1792 }, { "eff2.EffectMiscValue_1", 4 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 64343: 26 fields, fnv d38ef595
                { 64343, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 12355 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12355: 49 fields, fnv 58da00a3
                { 12355, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 12 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 77 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 32 }, { "eff1.ImplicitTarget_0", 53 }, { "eff1.ImplicitTarget_1", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12357: 26 fields, fnv c557c869
                { 12357, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 64343 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 2094: 49 fields, fnv dc461f15
                { 2094, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -60 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (!atRead)
            {
                seen.reads["igniteHeld"] = target->GetSpellAuraHolder(kIgniteDot) ? 1 : 0;
                if (k == 0)
                {
                    PutReadyAura(target, kMoltenArmor, kPhaseAuraMs);
                    m_rec.Note("30482 put on the Camel: " + HolderOf(target, kMoltenArmor));
                }
                if (k == kBlindStart || k == kNovaStart)
                {
                    target->RemoveAurasDueToSpell(PhaseAura(k - 1));
                    PutReadyAura(target, PhaseAura(k), kPhaseAuraMs);
                    m_rec.Note(U(PhaseAura(k)) + " put on the Camel: " + HolderOf(target, PhaseAura(k)));
                }
                if (k >= kFirstStrike)
                {
                    const uint32 phaseAura = PhaseAura(k);
                    p->CastSpell(target, kMoltenArmorHit, true, NULL, p->GetAura(kMoltenArmor, EFFECT_INDEX_0));
                    seen.reads["heldAfterStrike"] = target->GetSpellAuraHolder(phaseAura) ? 1 : 0;
                    m_rec.Note("34913 cast at the Camel by call; " + U(phaseAura) + " on it: " + HolderOf(target, phaseAura));
                }
            }
            else
            {
                seen.reads["igniteHeld"] = std::max<int64>(seen.reads["igniteHeld"], target->GetSpellAuraHolder(kIgniteDot) ? 1 : 0);
                seen.reads["held"] = target->GetSpellAuraHolder(PhaseAura(k)) ? 1 : 0;
                seen.reads["impact"] = p->HasAura(kImpactBuff) ? 1 : 0;
                p->RemoveAurasDueToSpell(kImpactBuff);
            }
        }

        std::string Own(St const& st) const override
        {
            uint32 hits = 0;
            uint32 casts = 0;
            uint32 crits = 0;
            uint32 ignite = 0;
            uint32 impact = 0;
            uint32 answered = 0;
            uint32 novaHeld = 0;
            bool ok = true;
            std::string swings;
            for (uint32 k = 1; k < st.steps.size(); ++k)
            {
                Seen const& seen = st.seen[k];
                const bool swing = k <= kSwings;
                const bool strike = k >= kFirstStrike;
                const std::vector<std::string> lines = HitAmounts(k, kMoltenArmorHit);
                const bool hit = swing && seen.read && seen.healthAfter < seen.healthBefore;
                for (size_t i = 0; i < lines.size(); ++i)
                {
                    ++casts;
                    crits += !lines[i].empty() && lines[i][lines[i].size() - 1] == 'c' ? 1 : 0;
                }
                answered += OtherHits(k, kMoltenArmorHit);
                if (swing)
                {
                    hits += hit ? 1 : 0;
                    ok = ok && seen.read && (hit ? lines.size() == 1 : lines.empty());
                    impact += Kept(st, k, "impact") == 1 ? 1 : 0;
                    swings += "; " + U(k + 1) + (hit ? " hit" : " not hit") + ": 34913 [" + (lines.empty() ? std::string("") : lines[0]) + "]";
                }
                if (strike)
                {
                    ok = ok && lines.size() == 1 && Kept(st, k, "heldAfterStrike") == 1 && Kept(st, k, "held") == 1;
                    novaHeld += k >= kNovaStart && Kept(st, k, "heldAfterStrike") == 1 ? 1 : 0;
                }
                ignite += Kept(st, k, "igniteHeld") == 1 || Triggered(seen, kIgniteDot) ? 1 : 0;
            }
            std::string poly = U(Kept(st, kPolyStep, "held") == 1 ? 1 : 0);
            std::string blind;
            for (uint32 k = kFirstStrike; k < kNovaStart; ++k)
            {
                std::string& phase = k < kBlindStart ? poly : blind;
                phase += (phase.empty() ? "" : "+") + U(Kept(st, k, "heldAfterStrike") == 1 ? 1 : 0);
            }
            ok = ok && hits > 0 && casts == crits && ignite == 0 && impact > 0 && answered == 0 &&
                 Kept(st, kPolyStep, "held") == 1;
            const std::string value = "crits=" + U(crits) + "/" + U(casts) + ",ignite=" + U(ignite) + ",impact=" + U(impact) +
                                      ",answered=" + U(answered) + ",poly=" + poly + ",blind=" + blind +
                                      ",nova=" + U(novaHeld) + "/" + U(kNovaStrikes);
            const std::string detail = "each swing that hits him casts 34913 once and every one crits; no 12654 on the Camel or cast; Impact's "
                                       "64343 on him after at least one swing; the Camel deals no 34913; the Camel's 118 held after the "
                                       "cast and after each of " + U(kStrikes) + " strikes, its 2094 after each of " + U(kBlindStrikes) +
                                       ", its 122 after each of " + U(kNovaStrikes) + swings;
            return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kMoltenArmor = 30482;
        static constexpr uint32 kMoltenArmorHit = 34913;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kImpactBuff = 64343;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kBlind = 2094;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kSwings = 50;
        static constexpr uint32 kStrikes = 3;
        static constexpr uint32 kBlindStrikes = 3;
        static constexpr uint32 kNovaStrikes = 30;
        /// Increased Critical Spell's points on him: 10 above kCritAuraPoints, so the Camel's Molten Armor (-5) leaves
        /// every 34913 above 100%.
        static constexpr int32 kArmorCritAuraPoints = kCritAuraPoints + 10;
        static constexpr int32 kPhaseAuraMs = 120000;
        static constexpr uint32 kPolyStep = kSwings + 1;
        static constexpr uint32 kFirstStrike = kSwings + 2;
        static constexpr uint32 kBlindStart = kFirstStrike + kStrikes;
        static constexpr uint32 kNovaStart = kBlindStart + kBlindStrikes;

        /// The aura the Camel holds at step k: Polymorph, then Blind from kBlindStart, then Frost Nova from kNovaStart.
        static uint32 PhaseAura(uint32 k) { return k >= kNovaStart ? kFrostNova : k >= kBlindStart ? kBlind : kPolymorph; }

        /// The damage logs of `spell` in step k's windows that the mage did not deal.
        uint32 OtherHits(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            uint32 count = 0;
            const std::string marker = ".hit." + U(spell) + "=";
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
                    if (fields.size() == 1 && fields[0].find(marker) != std::string::npos &&
                        fields[0].compare(0, 9, "hit self.") != 0)
                    {
                        ++count;
                    }
                }
            }
            return count;
        }
    };

    /**
     * S989 `mage-fire-pyroblast-path`: the instant Pyroblast's way through the server, on a Fire mage staged as
     * MageFireProofScenario stages him who learned Pyroblast! (92315). Pyroblast! asks for Hot Streak (48108) on its
     * caster (SpellAuraRestrictions 1073, read by Spell::CheckCast), and Hot Streak's aura 332 names 92315 for the
     * spells of class mask C 0x1000, which Pyroblast 11366 carries and 92315 does not: the CAST_SPELL handler casts
     * 92315 when he sends 11366 under it (SpellHandlers::HandleCastSpell). Hot Streak is the scenario's caster aura, put
     * on him before each cast step that does not take it off. Steps, every cast one that must not crit:
     *  1. Precision (13843, +6 spell hit) cast at himself, so no cast misses the level-85 Camel;
     *  2. Pyroblast! with Hot Streak up, through the handler's calls: accepted and instant, its SMSG_SPELL_START with
     *     cast time 0 and its SMSG_SPELL_GO in the step's own window, its cost 0, its periodic holder on the Camel at
     *     the read;
     *  3. Pyroblast! with Hot Streak taken off: refused, SMSG_CAST_FAILED with SPELL_FAILED_CASTER_AURASTATE, no spell
     *     start and no spell go;
     *  4. Pyroblast sent as the client sends it, with Hot Streak up: the handler swaps it, so the spell start (cast time
     *     0) and the spell go of the step's cast count name 92315 in the step's own window; Hot Streak on him at the read
     *     is stated, the cast having used it up or not;
     *  5. Pyroblast sent as the client sends it, with Hot Streak taken off: no swap, the spell start of 11366 with its
     *     3500 ms cast time, its spell go after the step's own window.
     * Each hit of Pyroblast! and of Pyroblast: (1238..1572 + int(1197 x 1.545) = 1849) x 1.2875 = 3974..4404.
     *
     * `pyroblast`: `instant=<step 2's spell go>/<its spell start's cast time>,refused=<step 3's refusal>,
     * swapped=<step 4's spell go>,hotStreak=<gone or kept at step 4's read>,plain=<step 5's spell start>/<its cast
     * time>`; OK when they are 92315/0 in the step's own window with the cost 0 and the holder on the Camel,
     * CASTER_AURASTATE without a start or a go, 92315 started with cast time 0 in the step's own window, and
     * 11366/3500 with its go after the step's own window. Hot Streak is stated, not judged, and so are the hits
     * against their band.
     */
    class MageFirePyroblastPath : public MageFireProofScenario
    {
    public:
        MageFirePyroblastPath() : MageFireProofScenario("mage-fire-pyroblast-path", 989, "pyroblast") {}

    protected:
        std::vector<uint32> Spells() const override { return { kPyroblastInstant }; }
        uint32 CasterAura() const override { return kHotStreak; }
        uint32 Watched() const override { return kHotStreak; }

        std::vector<Step> Steps() const override
        {
            Step refused = Cast(kPyroblastInstant, false);
            refused.noCasterAura = true;
            refused.refusal = true;
            Step swapped = Cast(kPyroblast, false);
            swapped.asClient = true;
            Step plain = Cast(kPyroblast, false);
            plain.asClient = true;
            plain.noCasterAura = true;
            plain.everyMs = kPlainEveryMs;
            return { SelfAura(kPrecision), Cast(kPyroblastInstant, false), refused, swapped, plain };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 92315: 49 fields, fnv 7ef90b4a
                { 92315, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (atRead && k == kInstantStep)
            {
                st.seen[k].reads["dot"] = target->GetSpellAuraHolder(kPyroblastInstant) ? 1 : 0;
                m_rec.Note("92315 on the Camel: " + HolderOf(target, kPyroblastInstant));
            }
        }

        std::string Own(St const& st) const override
        {
            const CastPath instant = CastPathOf(kInstantStep);
            const CastPath refused = CastPathOf(kRefusedStep);
            const CastPath swapped = CastPathOf(kSwappedStep);
            const CastPath plain = CastPathOf(kPlainStep);
            Seen const& instantSeen = st.seen[kInstantStep];
            Seen const& swappedSeen = st.seen[kSwappedStep];
            const std::string hotStreak = !swappedSeen.read ? "unread" : swappedSeen.holder == "none" ? "gone" : "kept";
            const std::string value = "instant=" + U(instant.go) + "/" + U(instant.castTime) + ",refused=" + CastResultName(refused.result) +
                                      ",swapped=" + U(swapped.go) + ",hotStreak=" + hotStreak + ",plain=" + U(plain.start) + "/" +
                                      U(plain.castTime);

            const bool instantOk = instant.start == kPyroblastInstant && instant.castTime == 0 && instant.startWindow == 0 &&
                                   instant.go == kPyroblastInstant && instant.goWindow == 0 && !instant.failed &&
                                   instantSeen.cost == 0 && Kept(st, kInstantStep, "dot") == 1;
            const bool refusedOk = refused.failed == kPyroblastInstant && refused.result == SPELL_FAILED_CASTER_AURASTATE && !refused.start &&
                                   !refused.go;
            const bool swappedOk = swapped.start == kPyroblastInstant && swapped.castTime == 0 && swapped.startWindow == 0 &&
                                   swapped.go == kPyroblastInstant && swapped.goWindow == 0 && !swapped.failed;
            const bool plainOk = plain.start == kPyroblast && plain.castTime == kPlainCastTime && plain.startWindow == 0 &&
                                 plain.go == kPyroblast && plain.goWindow > 0 && !plain.failed;

            std::string hits;
            const uint32 hitSteps[] = { kInstantStep, kSwappedStep, kPlainStep };
            for (const uint32 k : hitSteps)
            {
                Seen const& seen = st.seen[k];
                hits += (hits.empty() ? "" : ", ") + std::string("step ") + U(k + 1) + " " +
                        (!seen.logRead ? std::string("none") : U(seen.amount) + (seen.crit ? " crit" : "") +
                         (InBand(seen.amount, kHitLow, kHitHigh) ? " in" : " outside") + " the band");
            }
            const std::string detail =
                "step 2 " + PathText(instant) + ", cost " + U(instantSeen.cost) + ", 92315 on the Camel at the read " +
                (Kept(st, kInstantStep, "dot") == 1 ? "yes" : "no") + ", 48108 on him at the read " +
                (instantSeen.holder.empty() ? std::string("unread") : instantSeen.holder) + "; step 3 " + PathText(refused) +
                "; step 4, 11366 sent, " + PathText(swapped) + ", 48108 on him at the read " +
                (swappedSeen.holder.empty() ? std::string("unread") : swappedSeen.holder) +
                "; step 5, 11366 sent, " + PathText(plain) + "; the hits against " + U(kHitLow) + ".." + U(kHitHigh) + ": " + hits + ")";
            return value + (instantOk && refusedOk && swappedOk && plainOk ? "(OK: " : "(BUG: ") + detail;
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kPyroblastInstant = 92315;
        static constexpr uint32 kHotStreak = 48108;
        static constexpr uint32 kPlainCastTime = 3500;
        static constexpr uint32 kPlainEveryMs = 6000;
        static constexpr uint32 kHitLow = 3974;
        static constexpr uint32 kHitHigh = 4404;
        static constexpr uint32 kInstantStep = 1;
        static constexpr uint32 kRefusedStep = 2;
        static constexpr uint32 kSwappedStep = 3;
        static constexpr uint32 kPlainStep = 4;

        /// What a step's packets of its cast count held, as text.
        static std::string PathText(CastPath const& path)
        {
            return "start " + (path.start ? U(path.start) + " cast time " + U(path.castTime) + " in window " + std::to_string(path.startWindow)
                                          : std::string("none")) +
                   ", go " + (path.go ? U(path.go) + " in window " + std::to_string(path.goWindow) : std::string("none")) +
                   ", refusal " + (path.failed ? U(path.failed) + " " + CastResultName(path.result) : std::string("none"));
        }
    };

    /**
     * What the Molten Shields scenarios share (981, 982): a staged Fire mage with kSpellPower spell power, 1197 with
     * his intellect's, and FireTalents(), the fifteen points Molten Shields' tier asks, row by row: Burning Soul 3,
     * Improved Fire Blast 2 and Master of Elements 2; Fire Power 3 and Blazing Speed 2; Blast Wave and Improved Scorch
     * 2. Blazing Speed (31643) holds a root dispel of 100 in its second effect and a snare dispel of 0 in its third,
     * which Molten Shields (11094) raises by 100. The steps are casts by him at himself and the Camel's blows: his
     * Frostfire Bolt (44614) at himself has no flight, so its slow (-40%, the snare mechanic) and its hit reach him at
     * once; without the glyph the bolt has no DoT. Blazing Speed's own proc (31642) answers the melee and ranged hits
     * he takes, and none comes. At each read the scenario notes whether he holds Frostfire Bolt and Blazing Speed, and
     * the amount of his Mage Ward (543).
     */
    class MageMoltenShieldsScenario : public MageProcScenario
    {
    public:
        MageMoltenShieldsScenario(char const* name, int order, char const* ownCategory)
            : MageProcScenario(name, order, ownCategory) {}

    protected:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kBlazingSpeed = 31643;
        static constexpr uint32 kMageWard = 543;
        static constexpr int32 kSpellPower = 1000;

        /// The fifteen points before Molten Shields' tier, as (talent id, rank index).
        static std::vector<std::pair<uint32, uint32> > FireTalents()
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 } };
        }

        int32 SpellPower() const override { return kSpellPower; }
        bool TakesDamage() const override { return true; }

        /// `own` with the spells both scenarios reach: Frostfire Bolt and Blazing Speed, and the holders that can proc
        /// with what they cast: Blazing Speed's rank 2 (its trigger spell absent), Master of Elements' rank 2 and its
        /// mana, Offensive State and Arcane Missiles!, Deep Freeze Immunity State and its hit.
        static std::vector<SpellPrint> SharedPrints(std::vector<SpellPrint> own)
        {
            const std::vector<SpellPrint> shared = {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 31643: 72 fields, fnv b499aeb8
                { 31643, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 31 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 108 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 7 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 108 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 11 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
            own.insert(own.end(), shared.begin(), shared.end());
            return own;
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* /*target*/) override
        {
            Seen& seen = st.seen[k];
            Aura const* ward = p->GetAura(kMageWard, EFFECT_INDEX_0);
            if (!atRead)
            {
                seen.reads["wardAtCast"] = ward ? ward->GetModifier()->m_amount : -1;
                return;
            }
            seen.reads["slow"] = p->HasAura(kFrostfireBolt) ? 1 : 0;
            seen.reads["speed"] = p->HasAura(kBlazingSpeed) ? 1 : 0;
            seen.reads["ward"] = ward ? ward->GetModifier()->m_amount : -1;
            m_rec.Note("on him: " + U(kFrostfireBolt) + " " + HolderOf(p, kFrostfireBolt) + "; " + U(kBlazingSpeed) +
                       " " + HolderOf(p, kBlazingSpeed) + "; " + U(kMageWard) + " " + HolderOf(p, kMageWard) +
                       (ward ? ", amount " + std::to_string(ward->GetModifier()->m_amount) : std::string()));
        }

        /// What ReadOwn kept of step k under `name`; -2 when the step was not read.
        static int64 ReadAt(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return -2;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? -2 : i->second;
        }
    };

    /**
     * S981 `mage-fire-blazing-speed-dispel`: Blazing Speed's snare dispel on a staged Fire mage without Molten Shields,
     * whose text gives the snare removal to the talent: "your Blazing Speed also removes any movement slowing effects
     * when triggered". Steps: his Frostfire Bolt at himself, whose slow he then holds; Blazing Speed at himself, whose
     * snare dispel holds 0.
     *
     * `dispel`: `snare=<kept when Frostfire Bolt's holder is on him at Blazing Speed's read, else gone>`; OK at kept,
     * with the holder on him at the first read and Blazing Speed on him at the second.
     */
    class MageFireBlazingSpeedDispel : public MageMoltenShieldsScenario
    {
    public:
        MageFireBlazingSpeedDispel() : MageMoltenShieldsScenario("mage-fire-blazing-speed-dispel", 981, "dispel") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override { return FireTalents(); }

        std::vector<Step> Steps() const override
        {
            return { SelfAura(kFrostfireBolt), SelfAura(kBlazingSpeed) };
        }

        std::vector<SpellPrint> Prints() const override { return SharedPrints(std::vector<SpellPrint>()); }
        uint32 Watched() const override { return kFrostfireBolt; }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            const bool slowed = ReadAt(st, kSlowStep, "slow") == 1;
            const bool kept = ReadAt(st, kDispelStep, "slow") == 1;
            const bool sped = ReadAt(st, kDispelStep, "speed") == 1;
            const std::string value = std::string("snare=") + (kept ? "kept" : "gone");
            const std::string detail = U(kFrostfireBolt) + " on him at the first read: " + st.seen[kSlowStep].holder +
                                       "; at " + U(kBlazingSpeed) + "'s read: " + st.seen[kDispelStep].holder + ", " +
                                       U(kBlazingSpeed) + (sped ? " on him" : " not on him") +
                                       "; no Molten Shields, so the snare dispel holds 0";
            return value + (slowed && kept && sped ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr size_t kSteps = 2;
        static constexpr uint32 kSlowStep = 0;
        static constexpr uint32 kDispelStep = 1;
    };

    /**
     * S982 `mage-fire-mage-ward-break`: Molten Shields (11094) on a staged Fire mage, staged as 981 with Molten Shields
     * as the sixteenth point: "your Blazing Speed also removes any movement slowing effects when triggered and is also
     * triggered any time Mage Ward dissipates from absorbing damage". Steps:
     *  1. his Frostfire Bolt at himself, whose slow he then holds;
     *  2. Blazing Speed at himself: with Molten Shields its snare dispel holds 100, so the slow goes;
     *  3. Mage Ward (543) at himself, kWardEveryMs before the next step, so step 2's Blazing Speed has run out by then;
     *  4. the Camel's blow of kBlow fire damage at him: the ward absorbs its whole amount and is removed as broken, and
     *     the rest reaches him;
     *  5. Mage Ward at himself again, its cooldown taken off by call first, left to run out its 30 s;
     *  6. his health set to its maximum kExpireEveryMs after step 5, once the ward has run out, for a read.
     *
     * `ward`: `dispel=<gone when Frostfire Bolt is off him at step 2's read, else kept>,ward=<broken when step 3's ward
     * is off him at the blow's read and his health fell by the blow less that ward's amount, else held>,speed=<up when
     * the blow's windows sent Blazing Speed and it is on him at the blow's read, else none>,expire=<silent when no
     * Blazing Speed went in the windows of steps 5 and 6, step 5's ward was on him right after its cast, and neither
     * a ward nor Blazing Speed is on him at step 6's read, else cast>`; OK at gone, broken, up and silent, with
     * Frostfire Bolt on him at step 1's read.
     */
    class MageFireMageWardBreak : public MageMoltenShieldsScenario
    {
    public:
        MageFireMageWardBreak() : MageMoltenShieldsScenario("mage-fire-mage-ward-break", 982, "ward") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = FireTalents();
            talents.push_back(std::make_pair(kMoltenShieldsTalent, 0u));
            return talents;
        }

        std::vector<Step> Steps() const override
        {
            Step secondWard = SelfAura(kMageWard, kExpireEveryMs);
            secondWard.clearCooldown = true;
            return { SelfAura(kFrostfireBolt), SelfAura(kBlazingSpeed), SelfAura(kMageWard, kWardEveryMs),
                     Blow(kBlow, SPELL_SCHOOL_MASK_FIRE, kBlowEveryMs), secondWard, FullHealth(kLastEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return SharedPrints({
                // 543: 49 fields, fnv 94d386bd
                { 543, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 165 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 84 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAmplitude", 1065353216 }, { "eff1.EffectAura", 74 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 4 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            });
        }

        uint32 Watched() const override { return kBlazingSpeed; }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(" + U(st.steps.size()) + " steps, " + U(uint32(kSteps)) + " wanted)";
            }
            Seen const& blow = st.seen[kBlowStep];
            const int64 wardAmount = ReadAt(st, kWardStep, "ward");
            const int64 fell = int64(blow.healthBefore) - int64(blow.healthAfter);
            const bool slowed = ReadAt(st, kSlowStep, "slow") == 1;
            const bool gone = ReadAt(st, kDispelStep, "slow") == 0;
            const bool broken = wardAmount > 0 && blow.read && ReadAt(st, kBlowStep, "ward") == -1 &&
                                fell == int64(kBlow) - wardAmount;
            const bool up = Triggered(blow, kBlazingSpeed) && ReadAt(st, kBlowStep, "speed") == 1;
            const bool silent = !Triggered(st.seen[kExpireStep], kBlazingSpeed) &&
                                !Triggered(st.seen[kLastStep], kBlazingSpeed) && ReadAt(st, kExpireStep, "wardAtCast") > 0 &&
                                ReadAt(st, kLastStep, "ward") == -1 && ReadAt(st, kLastStep, "speed") == 0;
            const std::string value = std::string("dispel=") + (gone ? "gone" : "kept") + ",ward=" +
                                      (broken ? "broken" : "held") + ",speed=" + (up ? "up" : "none") + ",expire=" +
                                      (silent ? "silent" : "cast");
            const std::string detail = U(kFrostfireBolt) + " on him at step 1's read " + (slowed ? "yes" : "no") +
                                       "; the ward's amount at step 3's read " + std::to_string(wardAmount) +
                                       ", his health around the blow of " + U(kBlow) + " " + U(blow.healthBefore) + "->" +
                                       U(blow.healthAfter) + " (fell " + std::to_string(fell) + "); " + U(kBlazingSpeed) +
                                       " on him at the blow's read: " + blow.holder + "; the ward right after step 5's cast " +
                                       std::to_string(ReadAt(st, kExpireStep, "wardAtCast")) + ", at step 6's read " +
                                       std::to_string(ReadAt(st, kLastStep, "ward")) + ", and " + U(kBlazingSpeed) +
                                       " at step 6's read " + st.seen[kLastStep].holder;
            return value + (slowed && gone && broken && up && silent ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kMoltenShieldsTalent = 10543;
        static constexpr uint32 kBlow = 4000;
        static constexpr uint32 kWardEveryMs = 6000;
        static constexpr uint32 kBlowEveryMs = 4100;
        static constexpr uint32 kExpireEveryMs = 31000;
        static constexpr uint32 kLastEveryMs = 1000;
        static constexpr size_t kSteps = 6;
        static constexpr uint32 kSlowStep = 0;
        static constexpr uint32 kDispelStep = 1;
        static constexpr uint32 kWardStep = 2;
        static constexpr uint32 kBlowStep = 3;
        static constexpr uint32 kExpireStep = 4;
        static constexpr uint32 kLastStep = 5;
    };

    /**
     * S971 `mage-fire-flame-orb`: Flame Orb (82731) on a staged Fire mage with Fire Power 3 (54734, whose proc
     * chance, 100, is the orb's chance to explode). Staged: Burning Soul 3, Improved Fire Blast 2 and Fire Power 3;
     * Flame Orb learned. The Camel stands ten yards in front of him and casts Hit Me (100446, +20% spell hit
     * for whoever casts at it) on itself at its spawn: the orb's hit and its explosion are the orb's own rolls, at his
     * level (96% against a level-85 Camel), which the Camel's aura caps at 100%; an aura on him would not reach them.
     * One step: Flame Orb, an aura cast under the crit aura at +100, so every hit and the explosion crit; kStepMs to
     * the end. His 82731 casts the summon 84765, which puts the orb 44214 at his point for 18 s, flying ahead at
     * 10 yards a second; the orb's aura 82690 casts its scan 82734 every 500 ms, which snares the orb (82736) on its
     * first enemy within 10 yards and casts the orb's hit 82739 at the closest enemy once a second while the snare
     * holds; when the snare runs out the orb casts Fire Power's explosion 83619 and goes.
     *
     * The scan is centred on the orb's placement, which the engine writes from its spline every 400 ms of flight
     * (Unit::UpdateSplineMovement). The orb's flight starts 100 ms after the cast and the placement is written at
     * 200 ms (1 yard out), 600 ms (5), 1000 ms (9), and so on; the scans come at 500 ms, 1000 ms, 1500 ms. So the
     * first scan is centred 1 yard out while the orb itself is 4 yards out, and each later scan is centred a few
     * yards behind the orb the same way. A Camel 10 yards out, about 0.9 yards off the orb's line, is 9.05
     * yards from the first scan's centre: the orb finds it there, 7 yards ahead, slows to 1 yard a second from
     * its fourth yard, makes its fifteenth hit 14 s later from about 18 yards out (8 from the Camel) and explodes
     * a second after that from about 19 (9 from the Camel), both within the 10 yards of the hit's scan and the
     * explosion. The early reads note the placement and the spline position around the first two scans.
     *
     * His orb is the one creature 44214 within kOrbSearchRange of him: kEarlyReadsMs after the cast its placement, its
     * spline position, the Camel's distance and its snare are noted; at the step's read its holders of 82690 and 82736
     * and its run speed rate are read; kLateReadMs after the cast, between its last hit and the snare's end, its
     * position is noted; and at the end it is looked for again.
     *
     * `orb`: `hits=<82739 damage logs on the Camel> explosion=<83619 damage logs on the Camel> snared=<yes when the
     * orb held 82736 at the read> hover=<yes when its movement flags carried MOVEFLAG_HOVER at the read>
     * height=<its UNIT_FIELD_HOVERHEIGHT at the read> lift=<its z over the ground under it kLiftReadMs after the
     * cast, in its first tick, where the placement is still the lifted spawn>`; OK when there are kOrbHits hits,
     * each his and a crit of kHitLow..kHitHigh, one explosion, his and a crit of kBlastLow..kBlastHigh, the orb at
     * the read holds 82690 and 82736 at a run speed rate of its template's SpeedRun times kSnareRate, it hovers at a
     * height of 1.0, its lift is a hover height, 1.00 within kLiftTolerance, and no orb is left at the end.
     *
     * The bands, at kSpellPower staged (fire spell power 1197 with his intellect): the base damage is the orb's, at
     * the level it takes from its owner, and the mage supplies the spell power, the done mods and the crit. A hit is
     * (228..292, the hit's scaling 0.278 of the level-85 value 937.33 with a quarter of variance, + int(1197 x 0.134)
     * = 160) x 1.25 (Fire Specialization) x 1.03
     * (Fire Power), truncated, 499..581, then the crit: half again and 33% of that, 994..1158. The explosion is
     * (1134..1336, 1.318 of the same value with 0.164 of variance, + int(1197 x 0.193) = 231) x 1.2875, 1757..2017,
     * then the crit, 3504..4023.
     */
    class MageFireFlameOrb : public MageProcScenario
    {
    public:
        MageFireFlameOrb() : MageFireFlameOrb("mage-fire-flame-orb", 971, "orb") {}

        void Prepare() override
        {
            m_read = OrbRead();
            m_orbShare = Combat::OwnerHitShare();
            m_lift = 0.0f;
            m_liftRead = false;
            m_player = ObjectGuid();
            m_camel = ObjectGuid();
            MageProcScenario::Prepare();
        }

    protected:
        MageFireFlameOrb(char const* name, int order, char const* category) : MageProcScenario(name, order, category) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFlameOrb }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }

        std::vector<Step> Steps() const override
        {
            Step cast = AuraCast(kFlameOrb, kStepMs);
            cast.crit = true;
            return { cast };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84765: 26 fields, fnv 83712bbf
                { 84765, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44214 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82690: 49 fields, fnv 738c6c61
                { 82690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 82734 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 104 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82734: 26 fields, fnv af0731ee
                { 82734, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82739: 26 fields, fnv 47d44820
                { 82739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 174 }, { "eff0.EffectBonusCoefficient", 1040791372 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 22 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83619: 26 fields, fnv dea68555
                { 83619, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54734: 26 fields, fnv 601fb247
                { 54734, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 4 }, { "eff0.EffectSpellClassMask.Flags", 17592186177537 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0)
            {
                return;
            }
            m_player = st.player;
            m_camel = st.target;
            if (!atRead)
            {
                At(kLiftReadMs, [this]() { ReadLift(); });
                for (uint32 ms : kEarlyReadsMs)
                {
                    At(ms, [this, ms]() { ReadEarly(ms); });
                }
                return;
            }
            m_read = OrbRead();
            m_read.done = true;
            Creature* orb = LiveOrb(m_read.alive);
            if (orb && m_read.alive == 1)
            {
                m_read.scan = orb->HasAura(kOrbAura);
                m_read.snared = orb->HasAura(kSnare);
                m_read.runRate = orb->GetSpeedRate(MOVE_RUN);
                m_read.hover = (orb->WireFlags() & MOVEFLAG_HOVER) != 0;
                m_read.height = orb->GetFloatValue(UNIT_FIELD_HOVERHEIGHT);
                m_orbShare = Combat::OwnerShareOf(*orb);
            }
            At(kLateReadMs - CastReadMs(), [this]() { ReadLate(); });

            char buf[320];
            if (!orb)
            {
                snprintf(buf, sizeof(buf), "no creature %u within %.0f yd of him", kOrb, kOrbSearchRange);
                m_rec.Note(buf);
                return;
            }
            Geometry::Placement const& spawn = orb->Spawn();
            snprintf(buf, sizeof(buf),
                     "%u creature(s) %u within %.0f yd of him; the orb spawned at (%.2f, %.2f, %.2f) facing %.4f, now at (%.2f, %.2f, %.2f), "
                     "%.2f yd from the Camel; %u %s, %u %s, run speed rate %.6f",
                     m_read.alive, kOrb, kOrbSearchRange, spawn.X(), spawn.Y(), spawn.Z(), spawn.Facing(), orb->Where().X(),
                     orb->Where().Y(), orb->Where().Z(), CamelDistance(*orb), kOrbAura, m_read.scan ? "held" : "not held", kSnare,
                     m_read.snared ? "held" : "not held", m_read.runRate);
            m_rec.Note(buf);
        }

        std::string Own(St const& st) const override
        {
            const uint64 camel = st.target.GetRawValue();
            const uint64 mage = st.player.GetRawValue();
            std::vector<Hit> hits, blasts;
            uint32 elsewhere = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (logs[i]->payload.empty() || !Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            (log.spell != kHit && log.spell != kBlast))
                        {
                            continue;
                        }
                        if (log.target != camel)
                        {
                            ++elsewhere;
                            continue;
                        }
                        const Hit hit = { log.damage, (log.hitInfo & 0x2) != 0, log.attacker == mage };
                        (log.spell == kHit ? hits : blasts).push_back(hit);
                    }
                }
            }

            uint32 left = 0;
            const std::vector<Creature*> orbs = Orbs(st.player);
            left = uint32(orbs.size());
            CreatureInfo const* info = ObjectMgr::GetCreatureTemplate(kOrb);
            const float wantedRate = info ? info->SpeedRun * kSnareRate : 0.0f;

            bool ok = hits.size() == kOrbHits && blasts.size() == 1 && elsewhere == 0 && left == 0 && m_read.done &&
                      m_read.alive == 1 && m_read.scan && m_read.snared && info && std::fabs(m_read.runRate - wantedRate) < kRateTolerance &&
                      m_read.hover && std::fabs(m_read.height - 1.0f) < kHeightTolerance && m_liftRead &&
                      std::fabs(m_lift - kLift) < kLiftTolerance;
            std::string hitText;
            for (size_t i = 0; i < hits.size(); ++i)
            {
                ok = ok && hits[i].crit && hits[i].his && hits[i].amount >= kHitLow && hits[i].amount <= kHitHigh;
                hitText += (i ? "/" : "") + U(hits[i].amount) + (hits[i].crit ? "c" : "") + (hits[i].his ? "" : "!");
            }
            std::string blastText;
            for (size_t i = 0; i < blasts.size(); ++i)
            {
                ok = ok && blasts[i].crit && blasts[i].his && blasts[i].amount >= kBlastLow && blasts[i].amount <= kBlastHigh;
                blastText += (i ? "/" : "") + U(blasts[i].amount) + (blasts[i].crit ? "c" : "") + (blasts[i].his ? "" : "!");
            }

            char hoverText[64];
            snprintf(hoverText, sizeof(hoverText), " hover=%s height=%.2f lift=%.2f", m_read.hover ? "yes" : "no", m_read.height, m_lift);
            const std::string value = "hits=" + U(uint32(hits.size())) + " explosion=" + U(uint32(blasts.size())) + " snared=" +
                                      (m_read.snared ? "yes" : "no") + hoverText;
            char buf[256];
            snprintf(buf, sizeof(buf), "; at the read %u orb(s), %u %s, %u %s, run speed rate %.6f against %.6f; %u orb(s) at the end",
                     m_read.alive, kOrbAura, m_read.scan ? "held" : "not held", kSnare, m_read.snared ? "held" : "not held",
                     m_read.runRate, wantedRate, left);
            const std::string detail = "the hits " + (hitText.empty() ? std::string("none") : hitText) + " (" + U(kHitLow) + ".." +
                                       U(kHitHigh) + "); the explosion " + (blastText.empty() ? std::string("none") : blastText) + " (" +
                                       U(kBlastLow) + ".." + U(kBlastHigh) + "); " + U(elsewhere) + " on other units" + buf;
            return (ok ? "OK(" : "BUG(") + value + "; " + detail + ")";
        }

        /// The share the orb held at the step's read.
        Combat::OwnerHitShare m_orbShare;

    private:
        /// What the step's read found of his orb.
        struct OrbRead
        {
            bool done = false;
            uint32 alive = 0;                           ///< the live creatures 44214 within kOrbSearchRange of him
            bool scan = false;                          ///< the orb held 82690
            bool snared = false;                        ///< the orb held 82736
            float runRate = 0.0f;                       ///< the orb's run speed rate
            bool hover = false;                         ///< the orb's movement flags carried MOVEFLAG_HOVER
            float height = 0.0f;                        ///< the orb's UNIT_FIELD_HOVERHEIGHT
        };

        /// One damage log of the orb's hit or explosion on the Camel.
        struct Hit
        {
            uint32 amount;
            bool crit;
            bool his;                                   ///< the log names him as the attacker
        };

        /// Every creature 44214 within kOrbSearchRange of the player `player`, found through his map's grid.
        std::vector<Creature*> Orbs(ObjectGuid const& player) const
        {
            std::vector<Creature*> out;
            Player* p = sPlayerRegistry.Find(player);
            if (!p || !p->IsInWorld())
            {
                return out;
            }
            std::list<Creature*> found;
            MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kOrb, kOrbSearchRange);
            MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
            Cell::VisitGridObjects(p, searcher, kOrbSearchRange);
            out.assign(found.begin(), found.end());
            return out;
        }

        /// The first live creature 44214 within kOrbSearchRange of him, and in `alive` how many there are; NULL for none.
        Creature* LiveOrb(uint32& alive) const
        {
            Creature* orb = NULL;
            alive = 0;
            const std::vector<Creature*> orbs = Orbs(m_player);
            for (Creature* found : orbs)
            {
                if (found->IsAlive())
                {
                    ++alive;
                    orb = orb ? orb : found;
                }
            }
            return orb;
        }

        /// The Camel's distance from the orb's placement, which the orb's scan and its explosion are centred on; -1
        /// when the Camel is gone.
        float CamelDistance(Creature const& orb) const
        {
            Creature* camel = Get(m_camel);
            return camel ? orb.Where().DistanceTo(camel->Where()) : -1.0f;
        }

        /// kLiftReadMs after the cast, in the orb's first tick: its z over the ground under it.
        void ReadLift()
        {
            uint32 alive = 0;
            Creature* orb = LiveOrb(alive);
            if (orb && sPlayerRegistry.Find(m_player))
            {
                m_lift = orb->Where().Z() - Ground(orb->Where().X(), orb->Where().Y(), orb->Where().Z());
                m_liftRead = true;
            }
        }

        /// `ms` after the cast, around the orb's first scans: the orb's placement, where its spline has it, the Camel's
        /// distance from the placement, and whether the orb holds its snare.
        void ReadEarly(uint32 ms)
        {
            uint32 alive = 0;
            Creature* orb = LiveOrb(alive);
            char buf[320];
            if (!orb)
            {
                snprintf(buf, sizeof(buf), "%u ms after the cast: no creature %u within %.0f yd of him", ms, kOrb, kOrbSearchRange);
                m_rec.Note(buf);
                return;
            }
            float sx = orb->Where().X();
            float sy = orb->Where().Y();
            float sz = orb->Where().Z();
            if (orb->movespline && !orb->movespline->Finalized())
            {
                Movement::Location const live = orb->movespline->ComputePosition();
                sx = live.x;
                sy = live.y;
                sz = live.z;
            }
            snprintf(buf, sizeof(buf),
                     "%u ms after the cast: the orb's placement (%.2f, %.2f, %.2f), %.2f yd from the Camel; its spline at (%.2f, %.2f, %.2f); %u %s",
                     ms, orb->Where().X(), orb->Where().Y(), orb->Where().Z(), CamelDistance(*orb), sx, sy, sz, kSnare,
                     orb->HasAura(kSnare) ? "held" : "not held");
            m_rec.Note(buf);
        }

        /// kLateReadMs after the cast, between the orb's last hit and the end of its snare: where the orb is.
        void ReadLate()
        {
            uint32 alive = 0;
            Creature* orb = LiveOrb(alive);
            char buf[256];
            if (orb)
            {
                snprintf(buf, sizeof(buf), "%u ms after the cast: %u orb(s); the orb at (%.2f, %.2f, %.2f), %.2f yd from the Camel, %u %s",
                         kLateReadMs, alive, orb->Where().X(), orb->Where().Y(), orb->Where().Z(), CamelDistance(*orb), kSnare,
                         orb->HasAura(kSnare) ? "held" : "not held");
            }
            else
            {
                snprintf(buf, sizeof(buf), "%u ms after the cast: no creature %u within %.0f yd of him", kLateReadMs, kOrb, kOrbSearchRange);
            }
            m_rec.Note(buf);
        }

        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kOrb = 44214;
        static constexpr uint32 kOrbAura = 82690;
        static constexpr uint32 kSnare = 82736;
        static constexpr uint32 kHit = 82739;
        static constexpr uint32 kBlast = 83619;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kSpellPower = 1000;
        static constexpr float kOrbSearchRange = 40.0f;
        static constexpr uint32 kStepMs = 20000;
        static constexpr uint32 kEarlyReadsMs[4] = { 400, 500, 900, 1000 };
        static constexpr uint32 kLateReadMs = 15500;
        static constexpr size_t kOrbHits = 15;
        static constexpr uint32 kHitLow = 994;
        static constexpr uint32 kHitHigh = 1158;
        static constexpr uint32 kBlastLow = 3504;
        static constexpr uint32 kBlastHigh = 4023;
        static constexpr float kSnareRate = 0.1f;
        static constexpr float kRateTolerance = 0.0001f;
        static constexpr float kHeightTolerance = 0.001f;
        static constexpr uint32 kLiftReadMs = 10;
        static constexpr float kLift = 1.0f;
        static constexpr float kLiftTolerance = 0.01f;

        OrbRead m_read;
        float m_lift = 0.0f;                            ///< the orb's z over the ground under it, in its first tick
        bool m_liftRead = false;                        ///< the first-tick read found the orb
        ObjectGuid m_player;
        ObjectGuid m_camel;
    };

    /**
     * S973 `mage-fire-flame-orb-skips`: Flame Orb's two target rules. A staged Fire mage without Fire Power (Burning
     * Soul 3 alone, so the orb never explodes) casts Polymorph (118) on a second Camel and then Flame Orb. Beside the
     * first Camel, ten yards out with Hit Me (100446) on it as in 971, stand the second Camel six yards out and a
     * Chicken (621, a critter, hostile to him) three yards out, both nearer the orb's line than the first Camel: the
     * second Camel 5 yards from the first scan's centre (1 yard out) and the Chicken 2, the first Camel 9.2. The orb's
     * scan skips the critter and the polymorphed Camel and finds the first Camel, and its hits go to the first Camel
     * alone.
     *
     * `skips`: `hits=<82739 damage logs on the first Camel> sheep=<up when the second Camel holds 118 at the end>
     * critter=<unhurt when the Chicken is alive at its full health at the end> elsewhere=<82739 and 83619 damage logs
     * on any other unit>`; OK when there are kOrbHits hits, the second Camel holds 118, the Chicken is unhurt,
     * nothing elsewhere, no 83619 log on the first Camel and no orb is left at the end.
     */
    class MageFireFlameOrbSkips : public MageProcScenario
    {
    public:
        MageFireFlameOrbSkips() : MageProcScenario("mage-fire-flame-orb-skips", 973, "skips") {}

        void Prepare() override
        {
            m_chicken = ObjectGuid();
            MageProcScenario::Prepare();
            // The base schedules its steps by time, so none has run when it returns.
            Creature* chicken = Spawn(kChicken, P0.x + 3.0f, P0.y, Ground(P0.x + 3.0f, P0.y, P0.z), 3.14159f);
            if (chicken)
            {
                chicken->SetDefaultMovementType(CREATURE_MOVEMENT_IDLE);
                chicken->GetMotionMaster()->Initialize();
                Silence(chicken);
                m_chicken = chicken->GetObjectGuid();
            }
        }

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFlameOrb, kPolymorph }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { -4.0f, 0.0f } };
        }

        std::vector<Step> Steps() const override
        {
            Step orb = AuraCast(kFlameOrb, kStepMs);
            orb.crit = true;
            return { AuraCastAt(kPolymorph, 1, kPolymorphEveryMs), orb };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84765: 26 fields, fnv 83712bbf
                { 84765, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44214 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82690: 49 fields, fnv 738c6c61
                { 82690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 82734 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 104 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82734: 26 fields, fnv af0731ee
                { 82734, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82739: 26 fields, fnv 47d44820
                { 82739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 174 }, { "eff0.EffectBonusCoefficient", 1040791372 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 22 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            const uint64 first = st.targets.empty() ? 0 : st.targets[0].GetRawValue();
            uint32 hits = 0, blasts = 0, elsewhere = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (logs[i]->payload.empty() || !Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            (log.spell != kHit && log.spell != kBlast))
                        {
                            continue;
                        }
                        if (log.target != first)
                        {
                            ++elsewhere;
                        }
                        else if (log.spell == kHit)
                        {
                            ++hits;
                        }
                        else
                        {
                            ++blasts;
                        }
                    }
                }
            }

            Creature* sheep = st.targets.size() > 1 ? Get(st.targets[1]) : NULL;
            const bool sheepUp = sheep && sheep->HasAura(kPolymorph);
            Creature* chicken = Get(m_chicken);
            const bool unhurt = chicken && chicken->IsAlive() && chicken->GetHealth() == chicken->GetMaxHealth();
            uint32 left = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                std::list<Creature*> found;
                MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kOrb, kOrbSearchRange);
                MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
                Cell::VisitGridObjects(p, searcher, kOrbSearchRange);
                left = uint32(found.size());
            }

            const bool ok = hits == kOrbHits && blasts == 0 && elsewhere == 0 && sheepUp && unhurt && left == 0;
            const std::string value = "hits=" + U(hits) + " sheep=" + (sheepUp ? "up" : "down") + " critter=" + (unhurt ? "unhurt" : "hurt") +
                                      " elsewhere=" + U(elsewhere);
            return (ok ? "OK(" : "BUG(") + value + "; " + U(blasts) + " explosion log(s) on the first Camel; " + U(left) + " orb(s) at the end)";
        }

    private:
        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kOrb = 44214;
        static constexpr uint32 kHit = 82739;
        static constexpr uint32 kBlast = 83619;
        static constexpr uint32 kHitMe = 100446;
        static constexpr uint32 kChicken = 621;
        static constexpr int32 kSpellPower = 1000;
        static constexpr float kOrbSearchRange = 40.0f;
        static constexpr uint32 kStepMs = 20000;
        static constexpr uint32 kPolymorphEveryMs = 2000;
        static constexpr size_t kOrbHits = 15;

        ObjectGuid m_chicken;
    };

    /**
     * S974 `mage-fire-flame-orb-descends`: Flame Orb cast from a height. The mage of 971 (same talents, spells, spell
     * power and Camel) stands kPlayerLift yards above the ground (PlayerLift) and casts Flame Orb once. The summon puts
     * the orb at his exact point; the orb glides forward at its flight speed while it sinks on the slow-fall curve to
     * one hover height above the ground, then flies level from there on the same line. Reads at kEarlyReadsMs after the
     * cast and at the step's read: the orb's spline and placement, their distance ahead of the orb's spawn in the plane,
     * their z, and the orb's z over the ground under it.
     *
     * `descent`: `spawn=<the orb's placement z over the ground under it kStartReadMs after the cast, in the tick of
     * the orb's first update: after the glide is launched, before the placement moves, within 0.01 of kPlayerLift>
     * ahead400=<the spline's distance ahead of the spawn at the first read, kAheadLow..kAheadHigh>
     * sunk400=<the spline's z below the spawn's at the first read, kSunkLow..kSunkHigh>
     * ahead500=<the same distance at 500 ms, kAhead500Low..kAhead500High: a fall without the slow flag would be done
     * sooner and ahead of it> landed=<the orb's z over the ground under it at the last early read, 1.00 within 0.01,
     * with the orb kLandedAhead yards or more ahead> flew=<yes when at the step's read the spline's z is within
     * kBandLow..kBandHigh over the ground under it (the orb a hover height over it, as the ground rises and falls
     * along the line), the placement is on the spawn's line within 0.01 and farther ahead than at the last early
     * read>`; OK when all hold. The orb's hits and explosion are counted in the detail, not pinned. The glide is
     * launched with the orb's first update, 100 ms after the cast, at 10 yd/s: at the first read it has run 0.3 s,
     * 3.00 yd ahead and 0.87 yd down the slow-fall curve.
     */
    class MageFireFlameOrbDescends : public MageProcScenario
    {
    public:
        MageFireFlameOrbDescends() : MageFireFlameOrbDescends("mage-fire-flame-orb-descends", 974, "descent") {}

        void Prepare() override
        {
            for (OrbSample& sample : m_early)
            {
                sample = OrbSample();
            }
            m_final = OrbSample();
            m_start = OrbSample();
            m_player = ObjectGuid();
            MageProcScenario::Prepare();
        }

    protected:
        MageFireFlameOrbDescends(char const* name, int order, char const* category) : MageProcScenario(name, order, category) {}

        float PlayerLift() const override { return kPlayerLift; }
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 11434, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFlameOrb }; }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 TargetAura() const override { return kHitMe; }

        std::vector<Step> Steps() const override
        {
            Step cast = AuraCast(kFlameOrb, kStepMs);
            cast.crit = true;
            return { cast };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 82731: 26 fields, fnv 53501c97
                { 82731, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84765: 26 fields, fnv 83712bbf
                { 84765, { { "eff0.present", 1 }, { "eff0.Effect", 28 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 44214 }, { "eff0.EffectMiscValue_1", 3019 }, { "eff0.EffectRadiusIndex_0", 36 }, { "eff0.ImplicitTarget_0", 47 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82690: 49 fields, fnv 738c6c61
                { 82690, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 23 }, { "eff0.EffectAuraPeriod", 500 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 82734 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 104 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 82734: 26 fields, fnv af0731ee
                { 82734, { { "eff0.present", 1 }, { "eff0.Effect", 3 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.ImplicitTarget_0", 18 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82736: 26 fields, fnv 63cfd739
                { 82736, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -90 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 82739: 26 fields, fnv 47d44820
                { 82739, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 174 }, { "eff0.EffectBonusCoefficient", 1040791372 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 22 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 83619: 26 fields, fnv dea68555
                { 83619, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1000 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.ImplicitTarget_0", 87 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 54734: 26 fields, fnv 601fb247
                { 54734, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 79 }, { "eff0.EffectBasePoints", 3 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 4 }, { "eff0.EffectSpellClassMask.Flags", 17592186177537 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 100446: 72 fields, fnv d831e1e8
                { 100446, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 49 }, { "eff0.EffectBasePoints", -10 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 184 }, { "eff1.EffectBasePoints", 20 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 186 }, { "eff2.EffectBasePoints", 20 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 127 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0)
            {
                return;
            }
            m_player = st.player;
            if (atRead)
            {
                Sample(m_final, "at the step's read");
                return;
            }
            At(kStartReadMs, [this]() { Sample(m_start, "at the start"); });
            for (size_t i = 0; i < kEarlyCount; ++i)
            {
                At(kEarlyReadsMs[i], [this, i]() { Sample(m_early[i], std::to_string(kEarlyReadsMs[i]) + " ms after the cast"); });
            }
        }

        std::string Own(St const& st) const override
        {
            uint32 hits = 0, blasts = 0;
            const uint64 camel = st.target.GetRawValue();
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (logs[i]->payload.empty() || !Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) ||
                            log.target != camel)
                        {
                            continue;
                        }
                        hits += log.spell == kHit ? 1 : 0;
                        blasts += log.spell == kBlast ? 1 : 0;
                    }
                }
            }

            const OrbSample& first = m_early[0];
            const OrbSample& last = m_early[kEarlyCount - 1];
            const OrbSample& second = m_early[1];
            const bool spawnOk = m_start.found && std::fabs(m_start.placeRise - kPlayerLift) < kTolerance;
            const bool ahead500Ok = second.found && second.splineAhead >= kAhead500Low && second.splineAhead <= kAhead500High;
            const bool aheadOk = first.found && first.splineAhead >= kAheadLow && first.splineAhead <= kAheadHigh;
            const bool sunkOk = first.found && first.splineSunk >= kSunkLow && first.splineSunk <= kSunkHigh;
            const bool landedOk = last.found && std::fabs(last.placeRise - 1.0f) < kTolerance && last.placeAhead >= kLandedAhead;
            const bool heightOk = m_final.splineRise >= kBandLow && m_final.splineRise <= kBandHigh;
            const bool flewOk = last.found && m_final.found && heightOk && std::fabs(m_final.placeSide) < kTolerance &&
                                m_final.placeAhead > last.placeAhead;

            char value[160];
            snprintf(value, sizeof(value), "spawn=%.2f ahead400=%.2f sunk400=%.2f ahead500=%.2f landed=%.2f flew=%s", m_start.placeRise,
                     first.splineAhead, first.splineSunk, second.splineAhead, last.placeRise, flewOk ? "yes" : "no");
            std::string reads;
            for (size_t i = 0; i < kEarlyCount; ++i)
            {
                reads += (i ? ", " : "") + std::to_string(kEarlyReadsMs[i]) + " ms: " + Describe(m_early[i]);
            }
            reads += ", at the step's read: " + Describe(m_final);
            return (spawnOk && aheadOk && sunkOk && ahead500Ok && landedOk && flewOk ? "OK(" : "BUG(") + std::string(value) + "; the reads: " + reads + "; " + U(hits) + " hit(s) and " + U(blasts) + " explosion(s) on the Camel)";
        }

    protected:
        /// What one read of the orb found.
        struct OrbSample
        {
            bool found = false;
            float spawnRise = 0.0f;                     ///< the orb's spawn z less the ground under its spawn
            float placeAhead = 0.0f;                    ///< its placement's distance from its spawn in the plane
            float placeZ = 0.0f;                        ///< its placement's z
            float placeRise = 0.0f;                     ///< its placement's z less the ground under it
            float placeSide = 0.0f;                     ///< its placement's distance from the line its spawn faces
            float splineAhead = 0.0f;                   ///< its spline's distance from its spawn in the plane
            float splineSunk = 0.0f;                    ///< its spawn's z less its spline's
            float splineRise = 0.0f;                    ///< its spline's z over the ground under the spline
            float spawnGround = 0.0f;                   ///< the ground under its spawn
            float placeGround = 0.0f;                   ///< the ground under its placement
            bool moving = false;                        ///< its spline was still running at the read
        };

        static std::string Describe(OrbSample const& s)
        {
            char buf[128];
            if (!s.found)
            {
                return "no orb";
            }
            snprintf(buf, sizeof(buf), "placement %.2f ahead at %.2f (%.2f over the ground), spline %.2f ahead and %.2f below the spawn", s.placeAhead,
                     s.placeZ, s.placeRise, s.splineAhead, s.splineSunk);
            return buf;
        }

        /// How far from him the orb is looked for, in yards.
        virtual float SearchRange() const { return kOrbSearchRange; }

        /// The first live creature 44214 within SearchRange of him, or NULL.
        Creature* FindOrb() const
        {
            Player* p = sPlayerRegistry.Find(m_player);
            if (!p || !p->IsInWorld())
            {
                return NULL;
            }
            std::list<Creature*> found;
            MaNGOS::AllCreaturesOfEntryInRangeCheck check(p, kOrb, SearchRange());
            MaNGOS::CreatureListSearcher<MaNGOS::AllCreaturesOfEntryInRangeCheck> searcher(found, check);
            Cell::VisitGridObjects(p, searcher, SearchRange());
            for (Creature* orb : found)
            {
                if (orb->IsAlive())
                {
                    return orb;
                }
            }
            return NULL;
        }

        /// Notes and keeps what the orb's placement and spline show against its spawn.
        void Sample(OrbSample& out, std::string const& when)
        {
            Creature* orb = FindOrb();
            if (!orb)
            {
                m_rec.Note(when + ": no creature " + U(kOrb) + " within " + U(uint32(SearchRange())) + " yd of him");
                return;
            }
            Geometry::Placement const& spawn = orb->Spawn();
            float sx = orb->Where().X();
            float sy = orb->Where().Y();
            float sz = orb->Where().Z();
            if (orb->movespline && !orb->movespline->Finalized())
            {
                Movement::Location const live = orb->movespline->ComputePosition();
                sx = live.x;
                sy = live.y;
                sz = live.z;
            }
            out.found = true;
            out.moving = orb->movespline && !orb->movespline->Finalized();
            out.spawnGround = Ground(spawn.X(), spawn.Y(), spawn.Z());
            out.spawnRise = spawn.Z() - out.spawnGround;
            out.placeAhead = std::hypot(orb->Where().X() - spawn.X(), orb->Where().Y() - spawn.Y());
            out.placeZ = orb->Where().Z();
            out.placeGround = Ground(orb->Where().X(), orb->Where().Y(), orb->Where().Z());
            out.placeRise = orb->Where().Z() - out.placeGround;
            out.placeSide = (orb->Where().Y() - spawn.Y()) * std::cos(spawn.Facing()) -
                            (orb->Where().X() - spawn.X()) * std::sin(spawn.Facing());
            out.splineAhead = std::hypot(sx - spawn.X(), sy - spawn.Y());
            out.splineSunk = spawn.Z() - sz;
            out.splineRise = sz - Ground(sx, sy, sz);
            m_rec.Note(when + ": " + Describe(out) + "; its spawn is " + std::to_string(out.spawnRise) + " over the ground");
        }

        static constexpr uint32 kFlameOrb = 82731;
        static constexpr uint32 kOrb = 44214;
        static constexpr uint32 kHit = 82739;
        static constexpr uint32 kBlast = 83619;
        static constexpr uint32 kHitMe = 100446;
        static constexpr int32 kSpellPower = 1000;
        static constexpr float kOrbSearchRange = 40.0f;
        static constexpr uint32 kStepMs = 20000;
        static constexpr float kPlayerLift = 5.0f;
        static constexpr float kTolerance = 0.01f;
        static constexpr float kAheadLow = 2.5f;
        static constexpr float kAheadHigh = 3.5f;
        static constexpr float kSunkLow = 0.6f;
        static constexpr float kSunkHigh = 1.1f;
        static constexpr float kLandedAhead = 5.0f;
        static constexpr float kBandLow = 0.75f;
        static constexpr float kBandHigh = 2.5f;
        static constexpr float kAhead500Low = 3.75f;
        static constexpr float kAhead500High = 4.25f;
        static constexpr uint32 kStartReadMs = 10;
        static constexpr size_t kEarlyCount = 4;
        static constexpr uint32 kEarlyReadsMs[4] = { 400, 500, 900, 1000 };

        OrbSample m_early[4];
        OrbSample m_final;
        OrbSample m_start;                              ///< its placement in the first update's tick, before it moves
        ObjectGuid m_player;
    };

    /**
     * S975 `mage-fire-flame-orb-downhill`: the glide that starts in flight. The mage of 974 stands 1.5 yards over the
     * ground (PlayerLift), which is under two hover heights, facing 247.5 degrees (mostly toward -y, 22.5 degrees off it
     * toward -x), where the plain falls away about 0.8 yd over its first 10 yd and 1.2 yd over 20 (it rises on most
     * other bearings). He casts Flame Orb once. The orb spawns level with him, a hover height and a half over the
     * ground: it does not glide at its start. It flies level until the floor under it is two hover heights below its
     * line, then glides forward on the slow-fall curve to a hover height over the floor ahead and flies level again, and
     * so on down the slope. Reads every kReadMs from kReadMs after the cast, kReads of them (the last within the 40 yd the
     * orb is looked for and before the plain rises back over the orb's line): the orb's spline and
     * placement, their distance ahead of the orb's spawn in the plane, the spline's z below the spawn's, and the
     * spline's z over the ground under it.
     *
     * `glide`: `level=<the spline's z below the spawn's at the first read, 0.00 within 0.01: no glide at the start>
     * first=<the first read the spline is more than kDrop below the spawn's z: the glide began in flight> ahead=<the
     * orb's distance ahead gained between that read and the one before it, over kAheadGain: it kept flying forward while it
     * sank> over=<the lowest and the highest height of the spline over the ground under it at any read, within
     * kRiseLow..kRiseHigh (2.5: two hover heights, and half of one for the floor's fall in a tick): it never went
     * under its hover line, and the floor falling away did not leave it further over it> line=<yes when every read
     * has the placement on the spawn's line within 0.01>`, and the orb flies on after it lands: the read after
     * `first` has its spline farther ahead than `first`'s, and the last read has the placement farther ahead than at
     * the `first` read and at the same z within 0.01. OK when all hold. Hits are not read.
     */
    class MageFireFlameOrbDownhill : public MageFireFlameOrbDescends
    {
    public:
        MageFireFlameOrbDownhill() : MageFireFlameOrbDescends("mage-fire-flame-orb-downhill", 975, "glide") {}

        void Prepare() override
        {
            for (OrbSample& sample : m_samples)
            {
                sample = OrbSample();
            }
            MageFireFlameOrbDescends::Prepare();
        }

    protected:
        float PlayerLift() const override { return 1.5f; }
        float PlayerFacing() const override { return 4.31969f; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0 || atRead)
            {
                return;
            }
            m_player = st.player;
            for (size_t i = 0; i < kReads; ++i)
            {
                At(uint32(i + 1) * kReadMs, [this, i]() { Sample(m_samples[i], std::to_string((i + 1) * kReadMs) + " ms after the cast"); });
            }
        }

        std::string Own(St const& /*st*/) const override
        {
            const bool levelOk = m_samples[0].found && std::fabs(m_samples[0].splineSunk) < kTolerance;
            int first = -1;
            float lowest = 1000.0f;
            float highest = -1000.0f;
            bool lineOk = true;
            bool allFound = true;
            for (size_t i = 0; i < kReads; ++i)
            {
                OrbSample const& s = m_samples[i];
                if (!s.found)
                {
                    allFound = false;
                    continue;
                }
                if (first < 0 && s.splineSunk > kDrop)
                {
                    first = int(i);
                }
                lowest = std::min(lowest, s.splineRise);
                highest = std::max(highest, s.splineRise);
                lineOk = lineOk && std::fabs(s.placeSide) < kTolerance;
            }
            const float gained = first >= 1 ? m_samples[first].splineAhead - m_samples[first - 1].splineAhead : 0.0f;
            const bool glideOk = first >= 1 && gained >= kAheadGain;
            const bool overOk = allFound && lowest >= kRiseLow && highest <= kRiseHigh;
            const bool flewOk = first >= 0 && first + 1 < int(kReads) && m_samples[first].found &&
                                m_samples[first + 1].found && m_samples[kReads - 1].found &&
                                m_samples[first + 1].splineAhead > m_samples[first].splineAhead &&
                                std::fabs(m_samples[kReads - 1].placeZ - m_samples[first].placeZ) < kTolerance &&
                                m_samples[kReads - 1].placeAhead > m_samples[first].placeAhead;

            char value[200];
            snprintf(value, sizeof(value), "level=%.2f first=%d ahead=%.2f over=%.2f..%.2f line=%s", m_samples[0].splineSunk, first + 1, gained,
                     lowest, highest, lineOk ? "yes" : "no");
            std::string reads;
            for (size_t i = 0; i < kReads; ++i)
            {
                reads += (i ? ", " : "") + std::to_string((i + 1) * kReadMs) + " ms: " + Describe(m_samples[i]);
            }
            return (levelOk && glideOk && overOk && lineOk && flewOk ? "OK(" : "BUG(") + std::string(value) + "; the reads: " + reads + ")";
        }

    private:
        static constexpr size_t kReads = 6;
        static constexpr uint32 kReadMs = 400;
        static constexpr float kDrop = 0.1f;
        static constexpr float kAheadGain = 1.0f;
        static constexpr float kRiseLow = 0.99f;
        static constexpr float kRiseHigh = 2.5f;

        OrbSample m_samples[6];
    };

    /**
     * S976 `mage-fire-flame-orb-climbs`: the orb follows rising ground. The mage of 974 stands on the ground
     * (PlayerLift 0) facing 64 degrees (1.11701 rad), 26 degrees from +y toward +x, over a plain that rises 4.0 yd in
     * its first 30 yd and 6.5 by 50, falls back to 2.6 at 70, and rises again to 12.1 by 180, with no step over 1.8 yd
     * in 5 yd anywhere (a wall would stop a climb). His Camel stands kTargetDistance yd out on +x, 22 yd from the
     * line, so no scan of the orb finds it and the orb never snares. The orb is lifted to its hover line at its
     * spawn and flies its 18 s at 10 yd/s. It is read every kReadMs from kReadMs after the cast, kReads of them, the
     * first 98 yd of its flight for the first ten reads; the last two sit at about 107.7 yd, where the harness
     * stops updating the orb, so they read its last placement: the orb's spline and placement, their distance ahead
     * of the orb's spawn in the plane, the spline's z over the ground under it, whether the spline still ran, and the
     * placement's distance from the spawn's line.
     *
     * `climb`: `rise=<the ground under the orb at the last read less the ground under its spawn, kRiseMin or more:
     * the line rose> over=<the lowest and the highest height of the spline over the ground under it at any read,
     * within kBandLow..kBandHigh: a quarter hover height of sag under the hover line, and two hover heights over
     * it, with half of one for the floor's fall in a tick, where the glide takes over> pace=<the spline's distance
     * gained from the first read to the ninth, over their 8 s, kPaceMin or more: it keeps its pace on the slope>
     * standing=<the reads from the first to the ninth at which the spline had ended, 0: it never stood between
     * legs> line=<yes when every placement is on the spawn's line within 0.01>`; OK when every read found the orb
     * and all five hold. Hits are not read.
     */
    class MageFireFlameOrbClimbs : public MageFireFlameOrbDescends
    {
    public:
        MageFireFlameOrbClimbs() : MageFireFlameOrbDescends("mage-fire-flame-orb-climbs", 976, "climb") {}

        void Prepare() override
        {
            for (OrbSample& sample : m_samples)
            {
                sample = OrbSample();
            }
            MageFireFlameOrbDescends::Prepare();
        }

    protected:
        float PlayerLift() const override { return 0.0f; }
        float PlayerFacing() const override { return 1.11701f; }
        float TargetDistance() const override { return kTargetDistance; }
        float SearchRange() const override { return kSearchRange; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (k != 0 || atRead)
            {
                return;
            }
            m_player = st.player;
            for (size_t i = 0; i < kReads; ++i)
            {
                const std::string when = std::to_string((i + 1) * kReadMs) + " ms after the cast";
                At(uint32(i + 1) * kReadMs, [this, i, when]() { Sample(m_samples[i], when); });
            }
        }

        std::string Own(St const& /*st*/) const override
        {
            float lowest = 1000.0f;
            float highest = -1000.0f;
            bool lineOk = true;
            bool allFound = true;
            for (size_t i = 0; i < kReads; ++i)
            {
                OrbSample const& s = m_samples[i];
                if (!s.found)
                {
                    allFound = false;
                    continue;
                }
                lowest = std::min(lowest, s.splineRise);
                highest = std::max(highest, s.splineRise);
                lineOk = lineOk && std::fabs(s.placeSide) < kTolerance;
            }
            OrbSample const& last = m_samples[kReads - 1];
            const float rise = last.found ? last.placeGround - m_samples[0].spawnGround : 0.0f;
            const bool overOk = allFound && lowest >= kBandLow && highest <= kBandHigh;
            const bool riseOk = last.found && rise >= kRiseMin;
            const float pace = (m_samples[kPaceTo].splineAhead - m_samples[kPaceFrom].splineAhead) /
                               float((kPaceTo - kPaceFrom) * kReadMs / 1000);
            uint32 standing = 0;
            for (size_t i = kPaceFrom; i <= kPaceTo; ++i)
            {
                standing += m_samples[i].moving ? 0 : 1;
            }
            const bool paceOk = allFound && pace >= kPaceMin;

            char value[200];
            snprintf(value, sizeof(value), "rise=%.2f over=%.2f..%.2f pace=%.2f standing=%u line=%s", rise,
                     allFound ? lowest : 0.0f, allFound ? highest : 0.0f, pace, standing, lineOk ? "yes" : "no");
            std::string reads;
            for (size_t i = 0; i < kReads; ++i)
            {
                OrbSample const& s = m_samples[i];
                char buf[96];
                snprintf(buf, sizeof(buf), "%s%u ms: ", i ? ", " : "", uint32((i + 1) * kReadMs));
                reads += buf;
                snprintf(buf, sizeof(buf), "%.2f over the ground, %.2f ahead", s.splineRise, s.splineAhead);
                reads += s.found ? buf : "no orb";
            }
            const bool ok = allFound && overOk && lineOk && riseOk && paceOk && standing == 0;
            return (ok ? "OK(" : "BUG(") + std::string(value) + "; the reads: " + reads + ")";
        }

    private:
        static constexpr size_t kReads = 12;
        static constexpr uint32 kReadMs = 1000;
        static constexpr size_t kPaceFrom = 0;           ///< the read at 1000 ms
        static constexpr size_t kPaceTo = 8;             ///< the read at 9000 ms
        static constexpr float kPaceMin = 9.0f;          ///< the orb's 10 yd/s less the slope's cosine
        static constexpr float kTargetDistance = 25.0f;
        static constexpr float kSearchRange = 200.0f;
        static constexpr float kRiseMin = 3.0f;

        OrbSample m_samples[12];
    };

    /**
     * S980 `mage-fire-improved-flamestrike`: Improved Flamestrike (84674, rank 2 of 2, whose chance is 100) on a staged
     * Fire mage with kSpellPower spell power, 1197 with his intellect's, and no mastery. Staged: Burning Soul 3,
     * Improved Fire Blast 2 and Master of Elements 2; Fire Power 3 and Blazing Speed 2; Blast Wave, Improved Scorch 2
     * and Cauterize 2; Molten Shields, Combustion and Firestarter (the 20 points the fifth tier asks), then Improved
     * Flamestrike 2. Burning Soul names the Fire tree, which teaches Fire Specialization (+25% fire damage); with Fire
     * Power 3 every fire hit and tick is multiplied by 1.25 x 1.03 = 1.2875. Camel A stands ten yards in front of him,
     * Camels B and C kOffsetB and kOffsetC yards from it to either side, inside the 8 yards of Blast Wave and of
     * Flamestrike around it, and Camel D kOffsetD yards from it, beyond them (ExtraTargets). Before each cast he takes
     * Precision (13843), so no cast misses, and the crit aura at -100, so no hit or tick crits. Steps:
     *  1. Blast Wave (11113) at Camel D, which it hits alone: one enemy, so no free Flamestrike;
     *  2. its cooldown taken off him by call, Blast Wave at Camel A, which it hits with B and C: the free Flamestrike
     *     (88148) comes down there, hits the three, and its ground effect ticks on each every 2 s for 8 s, read for
     *     kLastStepMs.
     * The cast the harness builds carries a unit target and no destination, so the free Flamestrike's point is Camel
     * A's position. Blast Wave hits for (851..1003 + int(1197 x 0.143)) x 1.2875 = 1315..1511, the free Flamestrike
     * for (558..682 + int(1197 x 0.146)) x 1.2875 = 942..1102, and each of its ticks for (96 + int(1197 x 0.061)) x
     * 1.2875 = 217; the spell power shares are read from the effects' coefficients.
     *
     * `flamestrike`: `free=<the 88148 casts in step 1's windows>/<in step 2's>,hits=<its hits in step 1's
     * windows>/<in step 2's>,ticks=<its ticks in step 1's windows>/<in step 2's>`, a step with no 88148 cast written
     * `-`; OK when it reads free=-/1,hits=0/3,ticks=0/12, Blast Wave hit one Camel at step 1 and three at step 2, and
     * every hit and tick lies in its band, one point either side allowed for the float product of the percent auras.
     */
    class MageFireImprovedFlamestrike : public MageProcScenario
    {
    public:
        MageFireImprovedFlamestrike() : MageProcScenario("mage-fire-improved-flamestrike", 980, "flamestrike") {}

    protected:
        MageFireImprovedFlamestrike(char const* name, int order, char const* category) : MageProcScenario(name, order, category) {}

        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10545, 1 }, { 11434, 2 }, { 10555, 1 }, { 10551, 0 }, { 10547, 1 }, { 11433, 1 },
                     { 10543, 0 }, { 10561, 0 }, { 11431, 0 }, { 10734, 1 } };
        }
        int32 SpellPower() const override { return kSpellPower; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        std::vector<std::pair<float, float> > ExtraTargets() const override
        {
            return { { 0.0f, kOffsetB }, { 0.0f, -kOffsetC }, { 0.0f, kOffsetD } };
        }

        std::vector<Step> Steps() const override
        {
            Step alone = Cast(kBlastWave, false);
            alone.target = kCamelD;
            Step three = Cast(kBlastWave, false);
            three.target = kCamelA;
            three.clearCooldown = true;
            three.everyMs = kLastStepMs;
            return { alone, three };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11113: 49 fields, fnv ac35ef1b
                { 11113, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1041395352 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 33 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1065353216 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 33 }, { "eff1.EffectBasePoints", -70 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 11 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 16 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 88148: 49 fields, fnv d69ff162
                { 88148, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 51 }, { "eff0.EffectBonusCoefficient", 1041596678 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 17 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 27 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 2000 }, { "eff1.EffectBasePoints", 12 }, { "eff1.EffectBonusCoefficient", 1031396131 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 28 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 84674: 49 fields, fnv 2a349246
                { 84674, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 18 }, { "eff0.EffectSpellClassMask.Flags", 274877906944 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 108 }, { "eff1.EffectBasePoints", -100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 10 }, { "eff1.EffectSpellClassMask.Flags", 4 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 29075: 26 fields, fnv d8954794
                { 29075, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 30 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 29077: 26 fields, fnv a4e31d4d
                { 29077, { { "eff0.present", 1 }, { "eff0.Effect", 30 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        /// At each read: the free Flamestrike's holder on every Camel, noted.
        void ReadOwn(St& st, uint32 /*k*/, bool atRead, Player* /*p*/, Creature* /*target*/) override
        {
            if (!atRead)
            {
                return;
            }
            static char const* const names[] = { "A", "B", "C", "D" };
            std::string text;
            for (uint32 camel = 0; camel < st.targets.size() && camel < 4; ++camel)
            {
                text += (camel ? "; " : "") + std::string("Camel ") + names[camel] + " " + HolderOf(Get(st.targets[camel]), kFreeFlamestrike);
            }
            m_rec.Note(U(kFreeFlamestrike) + " on each Camel: " + text);
        }

        std::string Own(St const& st) const override
        {
            int32 spellPower = 0;
            if (Player* p = sPlayerRegistry.Find(st.player))
            {
                spellPower = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
            }
            const int32 blastShare = Share(spellPower, kBlastWave, EFFECT_INDEX_0);
            const int32 hitShare = Share(spellPower, kFreeFlamestrike, EFFECT_INDEX_0);
            const int32 tickShare = Share(spellPower, kFreeFlamestrike, EFFECT_INDEX_1);
            const uint32 blastLow = uint32(float(kBlastWaveLow + blastShare) * kDoneMultiplier);
            const uint32 blastHigh = uint32(float(kBlastWaveHigh + blastShare) * kDoneMultiplier);
            const uint32 hitLow = uint32(float(kFlamestrikeLow + hitShare) * kDoneMultiplier);
            const uint32 hitHigh = uint32(float(kFlamestrikeHigh + hitShare) * kDoneMultiplier);
            const uint32 tick = uint32(float(kFlamestrikeTick + tickShare) * kDoneMultiplier);

            static uint32 const wantedBlasts[] = { 1, 3 };
            static uint32 const wantedCasts[] = { 0, 1 };
            static uint32 const wantedHits[] = { 0, 3 };
            static uint32 const wantedTicks[] = { 0, 12 };
            bool ok = st.steps.size() == 2;
            std::string free, hits, ticks, detail;
            for (uint32 k = 0; k < st.steps.size() && k < 2; ++k)
            {
                const uint32 casts = uint32(std::count(st.seen[k].triggered.begin(), st.seen[k].triggered.end(), kFreeFlamestrike));
                const std::vector<std::pair<uint32, bool> > blasts = HitsOf(k, kBlastWave);
                const std::vector<std::pair<uint32, bool> > flames = HitsOf(k, kFreeFlamestrike);
                const std::vector<uint32> flameTicks = TickAmounts(k, kFreeFlamestrike);
                ok = ok && blasts.size() == wantedBlasts[k] && casts == wantedCasts[k] && flames.size() == wantedHits[k] &&
                     flameTicks.size() == wantedTicks[k];

                std::string blastText, flameText, tickText;
                for (size_t i = 0; i < blasts.size(); ++i)
                {
                    blastText += (i ? "+" : "") + U(blasts[i].first) + (blasts[i].second ? " crit" : "");
                    ok = ok && !blasts[i].second && InBand(blasts[i].first, blastLow, blastHigh);
                }
                for (size_t i = 0; i < flames.size(); ++i)
                {
                    flameText += (i ? "+" : "") + U(flames[i].first) + (flames[i].second ? " crit" : "");
                    ok = ok && !flames[i].second && InBand(flames[i].first, hitLow, hitHigh);
                }
                for (size_t i = 0; i < flameTicks.size(); ++i)
                {
                    tickText += (i ? "+" : "") + U(flameTicks[i]);
                    ok = ok && InBand(flameTicks[i], tick, tick);
                }

                const char* sep = k ? "/" : "";
                free += sep + (casts ? U(casts) : std::string("-"));
                hits += sep + U(uint32(flames.size()));
                ticks += sep + U(uint32(flameTicks.size()));
                detail += std::string(k ? "; " : "") + "step " + U(k + 1) + ": Blast Wave hit " + U(uint32(blasts.size())) + " Camel(s) (" +
                          (blastText.empty() ? std::string("none") : blastText) + "), 88148 cast " + U(casts) + " time(s), its hits " +
                          (flameText.empty() ? std::string("none") : flameText) + ", its ticks " +
                          (tickText.empty() ? std::string("none") : tickText);
            }

            const std::string value = "free=" + free + ",hits=" + hits + ",ticks=" + ticks;
            char buf[384];
            snprintf(buf, sizeof(buf),
                     "; fire spell power %d; Blast Wave (%d..%d + %d) x %.4f = %u..%u, the free Flamestrike (%d..%d + %d) x %.4f = "
                     "%u..%u, its tick (%d + %d) x %.4f = %u; wanted free=-/1,hits=0/3,ticks=0/12 with one Camel hit at step 1 and three "
                     "at step 2",
                     spellPower, kBlastWaveLow, kBlastWaveHigh, blastShare, kDoneMultiplier, blastLow, blastHigh, kFlamestrikeLow,
                     kFlamestrikeHigh, hitShare, kDoneMultiplier, hitLow, hitHigh, kFlamestrikeTick, tickShare, kDoneMultiplier, tick);
            return value + (ok ? "(OK: " : "(BUG: ") + detail + buf + ")";
        }

    protected:
        static constexpr uint32 kBlastWave = 11113;
        static constexpr uint32 kFreeFlamestrike = 88148;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kCamelA = 0;
        static constexpr uint32 kCamelD = 3;
        static constexpr float kOffsetB = 3.0f;
        static constexpr float kOffsetC = 5.0f;
        static constexpr float kOffsetD = 20.0f;
        static constexpr uint32 kLastStepMs = 10000;
        static constexpr int32 kBlastWaveLow = 851;
        static constexpr int32 kBlastWaveHigh = 1003;
        static constexpr int32 kFlamestrikeLow = 558;
        static constexpr int32 kFlamestrikeHigh = 682;
        static constexpr int32 kFlamestrikeTick = 96;
        static constexpr float kDoneMultiplier = 1.25f * 1.03f;

        /// int(spellPower x the bonus coefficient of `spell`'s effect `eff`), as the done bonus takes it; 0 when the
        /// spell store lacks the effect.
        static int32 Share(int32 spellPower, uint32 spell, SpellEffectIndex eff)
        {
            SpellEntry const* info = sSpellStore.LookupEntry(spell);
            SpellEffectEntry const* effect = info ? info->GetSpellEffect(eff) : NULL;
            return effect ? int32(spellPower * effect->EffectBonusCoefficient) : 0;
        }

        /// The amounts and crit flags of the damage logs of `spell` he dealt in step k's windows, in order.
        std::vector<std::pair<uint32, bool> > HitsOf(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<std::pair<uint32, bool> > hits;
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
                    for (size_t f = 0; f < fields.size(); ++f)
                    {
                        uint32 amount = 0, crit = 0;
                        if (fields[f].compare(0, hit.size(), hit) == 0 && sscanf(fields[f].c_str() + hit.size(), "%u/%u", &amount, &crit) == 2)
                        {
                            hits.push_back(std::make_pair(amount, crit != 0));
                        }
                    }
                }
            }
            return hits;
        }
    };

    /**
     * S991 `mage-fire-improved-flamestrike-rank-one`: 980's staging with Improved Flamestrike's first rank (84673, whose
     * chance is 50 and whose cast-time modifier on Flamestrike is -50%), and kBlasts Blast Waves at Camel A, each with
     * Blast Wave's cooldown taken off him by call first, so the seeded rolls decide which of them call the free
     * Flamestrike (88148). The free Flamestrike is instant at either rank: its SMSG_SPELL_GO comes in the same window
     * of a step as its Blast Wave's.
     *
     * `rankOne`: `casts=<the steps whose Blast Wave called 88148>/<kBlasts>,sameWindow=<of those, the steps whose
     * 88148 SMSG_SPELL_GO came in the window of its Blast Wave's>`; OK when at least one cast came, every one is in its
     * Blast Wave's window, and every Blast Wave hit the three Camels.
     */
    class MageFireImprovedFlamestrikeRankOne : public MageFireImprovedFlamestrike
    {
    public:
        MageFireImprovedFlamestrikeRankOne()
            : MageFireImprovedFlamestrike("mage-fire-improved-flamestrike-rank-one", 991, "rankOne") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            std::vector<std::pair<uint32, uint32> > talents = MageFireImprovedFlamestrike::Talents();
            talents.back().second = 0;
            return talents;
        }

        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            for (uint32 i = 0; i < kBlasts; ++i)
            {
                Step blast = Cast(kBlastWave, false);
                blast.target = kCamelA;
                blast.clearCooldown = true;
                steps.push_back(blast);
            }
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFireImprovedFlamestrike::Prints();
            for (size_t i = 0; i < prints.size(); ++i)
            {
                if (prints[i].spell == 84674)
                {
                    prints[i].spell = 84673;
                    prints[i].fields["eff0.EffectBasePoints"] = 50;
                    prints[i].fields["eff1.EffectBasePoints"] = -50;
                }
            }
            return prints;
        }

        std::string Own(St const& st) const override
        {
            uint32 casts = 0, same = 0, hitAll = 0;
            std::string detail;
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const int32 blastAt = GoWindowOf(k, kBlastWave);
                const int32 flameAt = GoWindowOf(k, kFreeFlamestrike);
                const bool hit = HitsOf(k, kBlastWave).size() == 3;
                hitAll += hit ? 1 : 0;
                if (flameAt >= 0)
                {
                    ++casts;
                    same += flameAt == blastAt ? 1 : 0;
                }
                detail += std::string(k ? "; " : "") + "step " + U(k + 1) + ": Blast Wave go in window " + std::to_string(blastAt) +
                          ", 88148 go in window " + std::to_string(flameAt) + ", Blast Wave hit " + U(uint32(HitsOf(k, kBlastWave).size())) +
                          " Camel(s)";
            }
            const bool ok = st.steps.size() == kBlasts && casts >= 1 && same == casts && hitAll == kBlasts;
            const std::string value = "casts=" + U(casts) + "/" + U(kBlasts) + ",sameWindow=" + U(same);
            return value + (ok ? "(OK: " : "(BUG: ") + detail + "; the windows are StepWindows' indexes, 0 the step's own)";
        }

    private:
        static constexpr uint32 kBlasts = 6;

        /// The index in StepWindows(k) of the window of the first SMSG_SPELL_GO of `spell` in step k; -1 when none came.
        int32 GoWindowOf(uint32 k, uint32 spell) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            const std::vector<std::string> windows = StepWindows(k);
            for (size_t w = 0; w < windows.size(); ++w)
            {
                std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(windows[w], SMSG_SPELL_GO);
                for (size_t i = 0; i < gos.size(); ++i)
                {
                    std::string text;
                    uint32 seenSpell = 0;
                    if (!gos[i]->payload.empty() && Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, text) &&
                        sscanf(text.c_str(), "spell=%u", &seenSpell) == 1 && seenSpell == spell)
                    {
                        return int32(w);
                    }
                }
            }
            return -1;
        }
    };

    /**
     * S990 `mage-fire-invisibility-fade`: Invisibility's fade (66) on a staged Fire mage who has Fire Blast at the Camel
     * on the Camel's threat list. 66 ticks every second for three seconds, each tick taking a tenth of the mage's
     * threat from every list that holds the mage; its expiry casts Invisibility (32612), whose sanctuary effect takes
     * the mage off every list and out of combat. Staged as 983 stages him (Precision before each cast, the crit aura at
     * -100). Steps (T the time of step 2):
     *  1. Fire Blast at the Camel: the mage enters its list;
     *  2. the fade, cast by him at himself, its ticks at T+1, T+2 and T+3 s and its expiry at T+3 s;
     *  3. a wait read at T+1.5 s and 4. a wait read at T+2.5 s, between ticks;
     *  5. a wait read at T+3.5 s, after the expiry, four seconds long so the Camel's evade ends before step 6;
     *  6. Fire Blast again: it removes 32612 and lands, so the mage is on the list once more;
     *  7. the fade again, 8. a wait read at T'+1.5 s, 9. a wait read at T'+2.5 s, 10. the fade cancelled at T'+2.7 s,
     *     before its third tick.
     * At each step's act and read the scenario keeps the mage's threat on the Camel, whether the Camel's list is
     * empty, whether he is in combat and whether he holds 66 and 32612; for each step it also keeps the last
     * SMSG_THREAT_UPDATE value the client was sent for the mage in the step's windows.
     *
     * `fade`: `fade=<t0>,<t1>,<t2>,end=<e>,<empty>,<combat>,<invis>,
     * cancel=<u0>,<u1>,<u2>,<u3>,<fade>,<invis>,<combat>,sent=<s1>,<s2>`;
     * OK when t1 is int(t0 x 0.9) and t2 int(t1 x 0.9), one point either side, e is 0 with the list empty, he out of
     * combat and holding 32612; u1 and u2 follow u0 the same way, u3 (the second fade's own read, after the cancel)
     * equals u2, 66 and 32612 are gone, and he is in combat with the list not empty; the last threat update the client
     * was sent through the first fade's second wait is above 0 and at most t1 + 1, and the last of the whole record is above
     * 0 and at most u1 + 1.
     */
    class MageFireInvisibilityFade : public MageProcScenario
    {
    public:
        MageFireInvisibilityFade() : MageProcScenario("mage-fire-invisibility-fade", 990, "fade") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10555, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFade, kFireBlast }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kFade; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step first = Cast(kFireBlast, false);
            first.clearCooldown = true;
            Step second = Cast(kFireBlast, false);
            second.clearCooldown = true;
            Step fade = SelfAura(kFade, 1500);
            Step fadeAgain = SelfAura(kFade, 1500);
            fadeAgain.clearCooldown = true;
            return { first, fade, Wait(1000), Wait(1000), Wait(4000), second, fadeAgain, Wait(1000), Wait(200),
                     SelfRemoval(kFade, kCastEveryMs) };
        }

        /// At each act (`atRead` false) and read: the mage's threat on the Camel (rounded), whether the Camel's list is
        /// empty, whether he is in combat and whether he holds 32612 and 66; kept in Seen::reads under `a_` for the act.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            const std::string prefix = atRead ? "" : "a_";
            std::map<std::string, int64>& reads = st.seen[k].reads;
            reads[prefix + "threat"] = int64(target->GetThreatManager().getThreat(p) + 0.5f);
            reads[prefix + "empty"] = target->GetThreatManager().isThreatListEmpty() ? 1 : 0;
            reads[prefix + "combat"] = p->IsInCombat() ? 1 : 0;
            reads[prefix + "invis"] = HolderOf(p, kInvisible) != "none" ? 1 : 0;
            reads[prefix + "fade"] = HolderOf(p, kFade) != "none" ? 1 : 0;
            m_rec.Note(std::string(atRead ? "at the read" : "at the act") + ": his threat on the Camel " + U(reads[prefix + "threat"]) +
                       ", its list " + (reads[prefix + "empty"] ? "empty" : "not empty") + ", he " + (reads[prefix + "combat"] ? "in" : "out of") +
                       " combat, 32612 " + HolderOf(p, kInvisible) + ", 66 " + HolderOf(p, kFade));
        }

        /// What ReadOwn kept under `key` for step k; -1 when unread.
        static int64 Kept(St const& st, uint32 k, char const* key)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(key);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// Whether `value` is within one point of `expected`.
        static bool Near(int64 value, int64 expected)
        {
            return value >= 0 && value + 1 >= expected && value <= expected + 1;
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 66: 49 fields, fnv a17ad32c
                { 66, { { "eff0.present", 0 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 35009 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.EffectBasePoints", 100 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectIndex", 2 } } },
                { 35009, { { "absent", 1 } } },
                // 32612: 72 fields, fnv c904b48b
                { 32612, { { "eff0.present", 1 }, { "eff0.Effect", 176 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 18 }, { "eff1.EffectBasePoints", 340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRealPointsPerLevel", 1084227584 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 335 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != 10)
            {
                return "BUG(steps)";
            }
            const int64 t0 = Kept(st, kFadeStep, "a_threat");
            const int64 t1 = Kept(st, kFirstWait, "a_threat");
            const int64 t2 = Kept(st, kSecondWait, "a_threat");
            const int64 e = Kept(st, kEndStep, "threat");
            const int64 endEmpty = Kept(st, kEndStep, "empty");
            const int64 endCombat = Kept(st, kEndStep, "combat");
            const int64 endInvis = Kept(st, kEndStep, "invis");
            const int64 u0 = Kept(st, kSecondHit, "threat");
            const int64 u1 = Kept(st, kCancelFirstWait, "a_threat");
            const int64 u2 = Kept(st, kCancelSecondWait, "a_threat");
            const int64 u3 = Kept(st, kFadeAgainStep, "threat");
            const int64 cancelFade = Kept(st, kCancelStep, "fade");
            const int64 cancelInvis = Kept(st, kCancelStep, "invis");
            const int64 cancelCombat = Kept(st, kCancelStep, "combat");
            const int64 cancelEmpty = Kept(st, kCancelStep, "empty");

            int64 sent1 = -1, sent2 = -1;
            for (uint32 k = 0; k <= kSecondWait; ++k)
            {
                sent1 = st.seen[k].threatUpdate >= 0 ? st.seen[k].threatUpdate : sent1;
            }
            for (uint32 k = 0; k < st.seen.size(); ++k)
            {
                sent2 = st.seen[k].threatUpdate >= 0 ? st.seen[k].threatUpdate : sent2;
            }

            const bool fadeOk = t0 > 0 && Near(t1, int64(t0 * 0.9) ) && Near(t2, int64(t1 * 0.9));
            const bool endOk = e == 0 && endEmpty == 1 && endCombat == 0 && endInvis == 1;
            const bool cancelOk = u0 > 0 && Near(u1, int64(u0 * 0.9)) && Near(u2, int64(u1 * 0.9)) && u3 == u2 && cancelFade == 0 &&
                                  cancelInvis == 0 && cancelCombat == 1 && cancelEmpty == 0;
            const bool sentOk = sent1 > 0 && sent1 <= t1 + 1 && sent2 > 0 && sent2 <= u1 + 1;

            Seen const& hit = st.seen[0];
            const std::string value = "fade=" + U(uint64(t0 < 0 ? 0 : t0)) + "," + U(uint64(t1 < 0 ? 0 : t1)) + "," + U(uint64(t2 < 0 ? 0 : t2)) +
                                      ",end=" + U(uint64(e < 0 ? 0 : e)) + "," + U(uint64(endEmpty < 0 ? 0 : endEmpty)) + "," +
                                      U(uint64(endCombat < 0 ? 0 : endCombat)) + "," + U(uint64(endInvis < 0 ? 0 : endInvis)) + ",cancel=" +
                                      U(uint64(u0 < 0 ? 0 : u0)) + "," + U(uint64(u1 < 0 ? 0 : u1)) + "," + U(uint64(u2 < 0 ? 0 : u2)) + "," +
                                      U(uint64(u3 < 0 ? 0 : u3)) + "," + U(uint64(cancelFade < 0 ? 0 : cancelFade)) + "," +
                                      U(uint64(cancelInvis < 0 ? 0 : cancelInvis)) + "," + U(uint64(cancelCombat < 0 ? 0 : cancelCombat)) +
                                      ",sent=" + U(uint64(sent1 < 0 ? 0 : sent1)) + "," + U(uint64(sent2 < 0 ? 0 : sent2));
            char buf[1280];
            snprintf(buf, sizeof(buf),
                     "Fire Blast's hit %s %u and his threat after it %lld; each tick keeps 90%% of what the list held, one point either side "
                     "(fade %s: int(%lld x 0.9) = %lld wanted at the first wait, int(%lld x 0.9) = %lld at the second); after the expiry %lld "
                     "with the list %s, he %s and 32612 %s (%s); the cancelled fade: int(%lld x 0.9) = %lld wanted at the first wait, "
                     "int(%lld x 0.9) = %lld at the second, %lld at the read of the fade it cancels, 66 %s, 32612 %s, he %s, "
                     "the list %s (%s); the last SMSG_THREAT_UPDATE the client was sent by "
                     "the second wait of the first fade carries %lld, wanted at most %lld (%s), "
                     "and by the end of the record %lld, wanted at most %lld (%s)",
                     hit.logRead ? "of" : "unread", hit.amount, (long long)t0, fadeOk ? "OK" : "BUG", (long long)t0, (long long)int64(t0 * 0.9),
                     (long long)t1, (long long)int64(t1 * 0.9), (long long)e, endEmpty == 1 ? "empty" : "not empty",
                     endCombat == 1 ? "in combat" : "out of combat", endInvis == 1 ? "held" : "gone", endOk ? "OK" : "BUG",
                     (long long)u0, (long long)int64(u0 * 0.9), (long long)u1, (long long)int64(u1 * 0.9), (long long)u3,
                     cancelFade == 1 ? "held" : "gone", cancelInvis == 1 ? "held" : "gone", cancelCombat == 1 ? "in combat" : "out of combat",
                     cancelEmpty == 1 ? "empty" : "not empty", cancelOk ? "OK" : "BUG",
                     (long long)sent1, (long long)t1 + 1, (sent1 > 0 && sent1 <= t1 + 1) ? "OK" : "BUG", (long long)sent2, (long long)u1 + 1,
                     (sent2 > 0 && sent2 <= u1 + 1) ? "OK" : "BUG");
            return value + ((fadeOk && endOk && cancelOk && sentOk) ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kFade = 66;
        static constexpr uint32 kInvisible = 32612;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kFadeStep = 1;
        static constexpr uint32 kFirstWait = 2;
        static constexpr uint32 kSecondWait = 3;
        static constexpr uint32 kEndStep = 4;
        static constexpr uint32 kSecondHit = 5;
        static constexpr uint32 kFadeAgainStep = 6;
        static constexpr uint32 kCancelFirstWait = 7;
        static constexpr uint32 kCancelSecondWait = 8;
        static constexpr uint32 kCancelStep = 9;
    };

    /**
     * S1124 `mage-threat-reduction`: Fade (586) on a staged Fire mage who has Fire Blast at the Camel on the Camel's
     * threat list. Fade is a priest spell he casts at himself, triggered; its one effect, aura 103 (MOD_TOTAL_THREAT)
     * at -90,000,000 under SPELL_ATTR_EX_NO_THREAT, is the threat effect Mirror Image (55342) and Shadowmeld (58984)
     * carry too: it asks every list that holds him to hold that much of his threat off for Fade's 10 s, and withdraws
     * the ask when Fade ends. Mirror Image is not cast: its Initialize Images (58836) holds spell_script_target rows,
     * which the guard refuses. Staged as 990 stages him (Precision before each cast, the crit aura at -100). Steps (F
     * the time of step 2):
     *  1. Fire Blast at the Camel: the mage enters its list;
     *  2. Fade, at F;
     *  3. Fire Blast again at F+1.5 s, under Fade, its cooldown taken off first;
     *  4. a wait at F+10.5 s, after Fade has ended at F+10 s;
     *  5. Fade again at F+11.5 s, its cooldown taken off first;
     *  6. a wait at F+22 s, after the second Fade has ended.
     * At each step's act and read the scenario keeps the mage's threat on the Camel, whether the Camel's list is empty
     * and whether he holds 586; for each step it also keeps the last SMSG_THREAT_UPDATE value the client was sent for
     * the mage in the step's windows.
     *
     * `threat`: `hit=<T>,held=<t2>,<t3>,listed=<l>,after=<t4>,fade=<t5>,<t6>,sent=<s>`: T at step 1's read, t2 at step
     * 2's act, t3 at step 3's read, l 1 when the Camel's list held him at step 2's act, t4 at step 4's act, t5 at step
     * 5's act, t6 at step 6's act, and s the last threat update the client was sent for him through step 3's windows.
     * OK when T is above 0; t2 and t3 are 0 and l is 1; t4 is above T and below 2.2 T (T and the second Fire Blast's
     * threat, which Fade held off); t5 is 0 and t6 equals t4; and s is 0.
     */
    class MageThreatReduction : public MageProcScenario
    {
    public:
        MageThreatReduction() : MageProcScenario("mage-threat-reduction", 1124, "threat") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10555, 1 } };
        }
        std::vector<uint32> Spells() const override { return { kFireBlast }; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kFade; }

        std::vector<Step> Steps() const override
        {
            Step first = Cast(kFireBlast, false);
            first.clearCooldown = true;
            Step heldHit = Cast(kFireBlast, false);
            heldHit.clearCooldown = true;
            heldHit.everyMs = kHeldHitToEndMs;
            Step fadeAgain = SelfAura(kFade, kFadeToEndMs);
            fadeAgain.clearCooldown = true;
            return { first, SelfAura(kFade, kFadeToHitMs), heldHit, Wait(kEndToFadeMs), fadeAgain, Wait(kEndToFadeMs) };
        }

        /// At each act (`atRead` false) and read: the mage's threat on the Camel (rounded), whether the Camel's list is
        /// empty and whether he holds 586; kept in Seen::reads under `a_` for the act.
        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            const std::string prefix = atRead ? "" : "a_";
            std::map<std::string, int64>& reads = st.seen[k].reads;
            reads[prefix + "threat"] = int64(target->GetThreatManager().getThreat(p) + 0.5f);
            reads[prefix + "empty"] = target->GetThreatManager().isThreatListEmpty() ? 1 : 0;
            reads[prefix + "fade"] = HolderOf(p, kFade) != "none" ? 1 : 0;
            m_rec.Note(std::string(atRead ? "at the read" : "at the act") + ": his threat on the Camel " +
                       U(reads[prefix + "threat"]) + ", its list " + (reads[prefix + "empty"] ? "empty" : "not empty") + ", 586 " +
                       HolderOf(p, kFade));
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 2136: 26 fields, fnv 30ec504c
                { 2136, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 23 }, { "eff0.EffectBonusCoefficient", 1054582243 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 586: 26 fields, fnv 94621339
                { 586, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 103 }, { "eff0.EffectBasePoints", -90000000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            };
        }

        std::string Own(St const& st) const override
        {
            if (st.steps.size() != kSteps)
            {
                return "BUG(steps)";
            }
            const int64 hit = ReadOf(st, kFirstHit, "threat");
            const int64 held = ReadOf(st, kFadeStep, "a_threat");
            const int64 heldHit = ReadOf(st, kHeldHit, "threat");
            const int64 listed = ReadOf(st, kFadeStep, "a_empty") == 0 ? 1 : 0;
            const int64 after = ReadOf(st, kAfterStep, "a_threat");
            const int64 fade = ReadOf(st, kFadeAgainStep, "a_threat");
            const int64 back = ReadOf(st, kBackStep, "a_threat");
            int64 sent = -1;
            for (uint32 k = 0; k <= kHeldHit; ++k)
            {
                sent = st.seen[k].threatUpdate >= 0 ? st.seen[k].threatUpdate : sent;
            }

            const bool hitOk = hit > 0;
            const bool heldOk = held == 0 && heldHit == 0 && listed == 1;
            const bool afterOk = after > hit && after * 10 < hit * 22;
            const bool fadeOk = fade == 0 && back >= 0 && back == after;
            const bool sentOk = sent == 0;

            Seen const& firstHit = st.seen[kFirstHit];
            const std::string value = "hit=" + Value(hit) + ",held=" + Value(held) + "," + Value(heldHit) + ",listed=" +
                                      Value(listed) + ",after=" + Value(after) + ",fade=" + Value(fade) + "," + Value(back) +
                                      ",sent=" + Value(sent);
            char buf[1024];
            snprintf(buf, sizeof(buf),
                     "Fire Blast's hit %s %u and his threat after it %lld (%s); under Fade his threat %lld at its act and %lld "
                     "after the Fire Blast under it, the Camel's list %s (%s); after Fade's end %lld, wanted above %lld and "
                     "below 2.2 x %lld = %lld (%s); the second Fade %lld at its act and %lld after its end, wanted 0 and %lld "
                     "(%s); the last SMSG_THREAT_UPDATE the client was sent for him through the Fire Blast under Fade carries "
                     "%lld, wanted 0 (%s)",
                     firstHit.logRead ? "of" : "unread", firstHit.amount, (long long)hit, hitOk ? "OK" : "BUG",
                     (long long)held, (long long)heldHit, listed == 1 ? "holding him" : "empty", heldOk ? "OK" : "BUG",
                     (long long)after, (long long)hit, (long long)hit, (long long)(hit * 22 / 10), afterOk ? "OK" : "BUG",
                     (long long)fade, (long long)back, (long long)after, fadeOk ? "OK" : "BUG", (long long)sent,
                     sentOk ? "OK" : "BUG");
            return value + ((hitOk && heldOk && afterOk && fadeOk && sentOk) ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        /// What ReadOwn kept under `key` for step k; -1 when unread.
        static int64 ReadOf(St const& st, uint32 k, char const* key)
        {
            if (k >= st.seen.size())
            {
                return -1;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(key);
            return i == st.seen[k].reads.end() ? -1 : i->second;
        }

        /// A read as the category writes it; 0 for one never made.
        static std::string Value(int64 read)
        {
            return U(uint64(read < 0 ? 0 : read));
        }

        static constexpr uint32 kFade = 586;
        static constexpr uint32 kFireBlast = 2136;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kFadeToHitMs = 1500;        ///< Fade to the Fire Blast under it
        static constexpr uint32 kHeldHitToEndMs = 9000;     ///< that Fire Blast to the wait after Fade's 10 s
        static constexpr uint32 kEndToFadeMs = 1000;        ///< a wait to the next step, or to the end of the record
        static constexpr uint32 kFadeToEndMs = 10500;       ///< the second Fade to the wait after its 10 s
        static constexpr size_t kSteps = 6;
        static constexpr uint32 kFirstHit = 0;
        static constexpr uint32 kFadeStep = 1;
        static constexpr uint32 kHeldHit = 2;
        static constexpr uint32 kAfterStep = 3;
        static constexpr uint32 kFadeAgainStep = 4;
        static constexpr uint32 kBackStep = 5;
    };

    /**
     * S1025 `mage-polymorph-damage`: Polymorph (118) and Wyvern Sting (19386) on the Camel break on any damage that
     * gets through, a direct hit or a periodic tick, and not on reactive damage, a heal or a hit the absorbs take
     * whole; a broken Wyvern Sting of his casts its damage over time (24131); Hex (51514) breaks on a chance per hit. A
     * Fire mage staged as MageFireProofScenario stages him, with Polymorph, Fireball (133), Living Bomb (44457) and
     * Molten Armor (30482) learned and Precision on him before each cast; no cast crits. Power Word: Shield (17),
     * Wyvern Sting and Hex are put on the Camel as PutReadyAura puts them, the shield at kShieldAmount and the Wyvern
     * Sting with him as its caster. Steps:
     *  1. Molten Armor cast at himself; the Camel takes him as its victim, so a confuse that ends sends it after him
     *     and not home, where it would evade the next cast;
     *  2.-3. Polymorph at the Camel; a Fireball at it;
     *  4.-7. Polymorph; Living Bomb at the sheep; a wait read before the bomb's first tick, and one read after it, at
     *     whose read the bomb is taken off the Camel;
     *  8.-10. Polymorph; Molten Armor's hit (34913) cast at the Camel by call as the armor's proc casts it; a Fireball;
     *  11.-14. Polymorph; the shield put on the Camel; a Fireball, which the shield takes whole; kSwings melee swings
     *     of his at the Camel by call (Unit::AttackerStateUpdate), which the shield takes whole too;
     *  15. the sheep's side of the proc call a Flash Heal (2061) of kHealAmount on it makes
     *     (Spell::DoAllEffectOnTarget: Unit::ProcDamageAndSpell with the taken-positive-spell flag);
     *  16.-17. Polymorph and the shield taken off the Camel and Wyvern Sting put on; a Fireball, at whose read the
     *     damage over time is taken off the Camel;
     *  18. kHexHits times: Hex put on the Camel when it does not hold it, then the proc call a Living Bomb tick of
     *     kTickDamage makes (Aura::PeriodicTick: Unit::ProcDamageAndSpell with the periodic and damage-taken flags).
     *
     * `polymorphDamage`: `direct=<118 after the Fireball>,tick=<118 before the tick>/<after it>,armor=<118 after
     * 34913>/<after the Fireball>,absorbed=<118 after the absorbed Fireball>/<after the absorbed swings>,heal=<118
     * after the heal>,sting=<19386 after the Fireball>,wakeUp=<24131 after it>,hex=<hits that took Hex off>/<hits>`, 1
     * for held and 0 for gone. OK when each Polymorph and the Wyvern Sting landed, the values read
     * `direct=0,tick=1/0,armor=1/0,absorbed=1/1,heal=1,sting=0,wakeUp=1`, the shield fell by the Fireball and by at
     * least one swing while the Camel's health did not fall, and Hex went on at least one hit and stayed on at least
     * one (a chance of kTickDamage / (25 x 85 - 150) a hit).
     */
    class MagePolymorphDamage : public MageFireProofScenario
    {
    public:
        MagePolymorphDamage() : MageFireProofScenario("mage-polymorph-damage", 1025, "polymorphDamage") {}

    protected:
        std::vector<uint32> Spells() const override { return { kPolymorph, kFireball, kLivingBomb, kMoltenArmor }; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 Watched() const override { return kPolymorph; }
        bool WatchedOnTarget() const override { return true; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<Step> Steps() const override
        {
            return { AuraCast(kMoltenArmor, 0),
                     AuraCast(kPolymorph, 0), Cast(kFireball, false),
                     AuraCast(kPolymorph, 0), AuraCast(kLivingBomb, kBombEveryMs), Wait(kBeforeTickEveryMs),
                     Wait(kSwingEveryMs),
                     AuraCast(kPolymorph, 0), Wait(kSwingEveryMs), Cast(kFireball, false),
                     AuraCast(kPolymorph, 0), Wait(kSwingEveryMs), Cast(kFireball, false), Wait(kSwingEveryMs),
                     Wait(kSwingEveryMs), Wait(kSwingEveryMs), Cast(kFireball, false),
                     Wait(kSwingEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 118: 49 fields, fnv ffbe2272
                { 118, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 56 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 16372 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 30482: 72 fields, fnv 920fa0aa
                { 30482, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 34913 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 197 }, { "eff1.EffectBasePoints", -5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 57 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 1792 }, { "eff2.EffectMiscValue_1", 4 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 34913: 26 fields, fnv aa3af5ec
                { 34913, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 75 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 17: 26 fields, fnv bff13de9
                { 17, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 19386: 26 fields, fnv 0a8cca4e
                { 19386, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 12 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 10 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 51514: 72 fields, fnv e2d49f1b
                { 51514, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 56 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 13321 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 260 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 754 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 2061: 26 fields, fnv b42e3764
                { 2061, { { "eff0.present", 1 }, { "eff0.Effect", 10 }, { "eff0.EffectAmplitude", 1065353216 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectBonusCoefficient", 1060739482 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 24131: 26 fields, fnv f85d4807
                { 24131, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (atRead)
            {
                seen.reads["poly"] = target->HasAura(kPolymorph) ? 1 : 0;
                seen.reads["sting"] = target->HasAura(kWyvernSting) ? 1 : 0;
                seen.reads["wakeUp"] = target->HasAura(kWyvernStingDot) ? 1 : 0;
                seen.reads["shield"] = ShieldLeft(target);
                seen.reads["health"] = target->GetHealth();
                if (k == kAfterTickStep)
                {
                    target->RemoveAurasDueToSpell(kLivingBomb);
                    m_rec.Note(U(kLivingBomb) + " taken off the Camel: " + HolderOf(target, kLivingBomb));
                }
                else if (k == kStingHit)
                {
                    m_rec.Note(U(kWyvernStingDot) + " on the Camel: " + HolderOf(target, kWyvernStingDot) +
                               "; taken off it");
                    target->RemoveAurasDueToSpell(kWyvernStingDot);
                }
                return;
            }
            seen.reads["healthBefore"] = target->GetHealth();
            seen.reads["shieldBefore"] = ShieldLeft(target);
            if (k == 0)
            {
                // a creature with no victim runs home when its confuse ends, and evades every spell on the way
                // (Unit::SetConfused, Creature::IsInEvadeMode); the silenced Camel takes the mage as its victim, as
                // a creature fighting him has him
                const bool attacking = target->Attack(p, false);
                m_rec.Note(std::string("the Camel takes him as its victim: ") + (attacking ? "it does" : "refused"));
            }
            else if (k == kStrikeStep)
            {
                p->CastSpell(target, kMoltenArmorHit, true, NULL, p->GetAura(kMoltenArmor, EFFECT_INDEX_0));
                m_rec.Note(U(kMoltenArmorHit) + " cast at the Camel by call; " + U(kPolymorph) + " on it: " +
                           HolderOf(target, kPolymorph));
            }
            else if (k == kShieldStep)
            {
                PutReadyAura(target, kShield, kAuraMs);
                if (Aura* shield = target->GetAura(kShield, EFFECT_INDEX_0))
                {
                    shield->GetModifier()->m_amount = kShieldAmount;
                }
                m_rec.Note(U(kShield) + " put on the Camel: " + HolderOf(target, kShield) + ", absorbing " +
                           std::to_string(ShieldLeft(target)));
            }
            else if (k == kSwingStep)
            {
                uint32 taken = 0;
                for (uint32 i = 0; i < kSwings; ++i)
                {
                    const int64 before = ShieldLeft(target);
                    p->AttackerStateUpdate(target);
                    taken += ShieldLeft(target) < before ? 1 : 0;
                }
                seen.reads["swingsTaken"] = taken;
                seen.reads["healthAfterSwings"] = target->GetHealth();
                m_rec.Note("his " + U(kSwings) + " swings at the Camel: the shield took " + U(taken) + ", " +
                           std::to_string(ShieldLeft(target)) + " left; " + U(kPolymorph) + " on it: " +
                           HolderOf(target, kPolymorph));
            }
            else if (k == kHealStep)
            {
                p->ProcDamageAndSpell(target, PROC_FLAG_NONE, PROC_FLAG_TAKEN_POSITIVE_SPELL, PROC_EX_NORMAL_HIT,
                                      kHealAmount, BASE_ATTACK, sSpellStore.LookupEntry(kFlashHeal));
                m_rec.Note("the sheep's proc of a heal of " + U(kHealAmount) + " by call; " + U(kPolymorph) +
                           " on it: " + HolderOf(target, kPolymorph));
            }
            else if (k == kStingStep)
            {
                target->RemoveAurasDueToSpell(kPolymorph);
                target->RemoveAurasDueToSpell(kShield);
                PutReadyAura(target, kWyvernSting, kAuraMs, p);
                seen.reads["stingPut"] = target->HasAura(kWyvernSting) ? 1 : 0;
                m_rec.Note(U(kPolymorph) + " and " + U(kShield) + " taken off the Camel; " + U(kWyvernSting) +
                           " put on it: " + HolderOf(target, kWyvernSting));
            }
            else if (k == kHexStep)
            {
                SpellEntry const* bomb = sSpellStore.LookupEntry(kLivingBomb);
                uint32 broke = 0;
                for (uint32 i = 0; i < kHexHits; ++i)
                {
                    if (!target->HasAura(kHex))
                    {
                        PutReadyAura(target, kHex, kAuraMs);
                    }
                    p->ProcDamageAndSpell(target, PROC_FLAG_ON_DO_PERIODIC,
                                          PROC_FLAG_ON_TAKE_PERIODIC | PROC_FLAG_TAKEN_ANY_DAMAGE, PROC_EX_NORMAL_HIT,
                                          kTickDamage, BASE_ATTACK, bomb);
                    broke += target->HasAura(kHex) ? 0 : 1;
                }
                seen.reads["hexBroke"] = broke;
                m_rec.Note(U(kHexHits) + " tick procs of " + U(kTickDamage) + " at the Camel under " + U(kHex) +
                           ": it went on " + U(broke));
            }
        }

        std::string Own(St const& st) const override
        {
            const uint32 polymorphs[] = { kFirstPoly, kTickPoly, kStrikePoly, kAbsorbPoly };
            bool landed = Kept(st, kStingStep, "stingPut") == 1;
            for (uint32 k : polymorphs)
            {
                landed = landed && Kept(st, k, "poly") == 1;
            }
            const int64 direct = Kept(st, kDirectHit, "poly");
            const int64 beforeTick = Kept(st, kBeforeTickStep, "poly");
            const int64 afterTick = Kept(st, kAfterTickStep, "poly");
            const int64 afterStrike = Kept(st, kStrikeStep, "poly");
            const int64 afterArmorHit = Kept(st, kStrikeHit, "poly");
            const int64 afterAbsorbedHit = Kept(st, kAbsorbedHit, "poly");
            const int64 afterSwings = Kept(st, kSwingStep, "poly");
            const int64 afterHeal = Kept(st, kHealStep, "poly");
            const int64 sting = Kept(st, kStingHit, "sting");
            const int64 wakeUp = Kept(st, kStingHit, "wakeUp");
            const int64 hexBroke = Kept(st, kHexStep, "hexBroke");
            const int64 shieldBefore = Kept(st, kAbsorbedHit, "shieldBefore");
            const int64 shieldAfter = Kept(st, kAbsorbedHit, "shield");
            const bool hitTaken = shieldAfter >= 0 && shieldAfter < shieldBefore &&
                                  Kept(st, kAbsorbedHit, "health") >= Kept(st, kAbsorbedHit, "healthBefore");
            const bool swingsTaken = Kept(st, kSwingStep, "swingsTaken") >= 1 &&
                                     Kept(st, kSwingStep, "healthAfterSwings") >= Kept(st, kSwingStep, "healthBefore");
            const bool ok = landed && direct == 0 && beforeTick == 1 && afterTick == 0 && afterStrike == 1 &&
                            afterArmorHit == 0 && afterAbsorbedHit == 1 && afterSwings == 1 && afterHeal == 1 &&
                            sting == 0 && wakeUp == 1 && hitTaken && swingsTaken && hexBroke >= 1 &&
                            hexBroke < int64(kHexHits);
            const std::string value = "direct=" + std::to_string(direct) +
                                      ",tick=" + std::to_string(beforeTick) + "/" + std::to_string(afterTick) +
                                      ",armor=" + std::to_string(afterStrike) + "/" + std::to_string(afterArmorHit) +
                                      ",absorbed=" + std::to_string(afterAbsorbedHit) + "/" +
                                      std::to_string(afterSwings) +
                                      ",heal=" + std::to_string(afterHeal) +
                                      ",sting=" + std::to_string(sting) + ",wakeUp=" + std::to_string(wakeUp) +
                                      ",hex=" + std::to_string(hexBroke) + "/" + U(kHexHits);
            const std::string detail =
                std::string("each Polymorph and the Wyvern Sting ") + (landed ? "landed" : "did not all land") +
                "; the shield read " + std::to_string(shieldBefore) + " before the absorbed Fireball and " +
                std::to_string(shieldAfter) + " after it, the Camel's health " +
                std::to_string(Kept(st, kAbsorbedHit, "healthBefore")) + " and " +
                std::to_string(Kept(st, kAbsorbedHit, "health")) + "; the shield took " +
                std::to_string(Kept(st, kSwingStep, "swingsTaken")) + " of " + U(kSwings) + " swings, the health " +
                std::to_string(Kept(st, kSwingStep, "healthBefore")) + " and " +
                std::to_string(Kept(st, kSwingStep, "healthAfterSwings")) +
                "; wanted direct=0,tick=1/0,armor=1/0,absorbed=1/1,heal=1,sting=0,wakeUp=1, the shield taking the "
                "Fireball and a swing with no health lost, and Hex both gone and kept";
            return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kPolymorph = 118;
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kMoltenArmor = 30482;
        static constexpr uint32 kMoltenArmorHit = 34913;
        static constexpr uint32 kShield = 17;
        static constexpr uint32 kWyvernSting = 19386;
        static constexpr uint32 kWyvernStingDot = 24131;
        static constexpr uint32 kHex = 51514;
        static constexpr uint32 kFlashHeal = 2061;
        static constexpr uint32 kHealAmount = 5000;
        static constexpr int32 kShieldAmount = 100000;
        static constexpr int32 kAuraMs = 60000;
        static constexpr uint32 kSwings = 6;
        static constexpr uint32 kHexHits = 40;
        static constexpr uint32 kTickDamage = 400;
        static constexpr uint32 kBombEveryMs = 2000;
        static constexpr uint32 kBeforeTickEveryMs = 1500;
        static constexpr uint32 kFirstPoly = 1;
        static constexpr uint32 kDirectHit = 2;
        static constexpr uint32 kTickPoly = 3;
        static constexpr uint32 kBeforeTickStep = 5;
        static constexpr uint32 kAfterTickStep = 6;
        static constexpr uint32 kStrikePoly = 7;
        static constexpr uint32 kStrikeStep = 8;
        static constexpr uint32 kStrikeHit = 9;
        static constexpr uint32 kAbsorbPoly = 10;
        static constexpr uint32 kShieldStep = 11;
        static constexpr uint32 kAbsorbedHit = 12;
        static constexpr uint32 kSwingStep = 13;
        static constexpr uint32 kHealStep = 14;
        static constexpr uint32 kStingStep = 15;
        static constexpr uint32 kStingHit = 16;
        static constexpr uint32 kHexStep = 17;

        /// What the Camel's Power Word: Shield has left to absorb; -1 when it holds none.
        static int64 ShieldLeft(Unit* unit)
        {
            Aura* shield = unit->GetAura(kShield, EFFECT_INDEX_0);
            return shield ? int64(shield->GetModifier()->m_amount) : -1;
        }
    };

    /**
     * S1026 `mage-threshold-prevented`: a root (Frost Nova, 122), a fear (Fear, 5782) and Hex's pacify-silence (51514)
     * on the Camel break on a hit by a chance that grows with the damage, and they count the damage a shield absorbed
     * and the damage the Camel's damage-reduction effects took off the hit. A Fire mage staged as MageFireProofScenario
     * stages him, with Fireball (133) learned and Precision on him before each cast; no cast crits. The Camel is level
     * 85, so a hit that counts 25 x 85 - 150 = 1975 or more breaks for certain. Frost Nova, Fear and Hex are put on the
     * Camel as PutReadyAura puts them with him as their caster, and so are Power Word: Shield (17) at kShieldAmount,
     * Shield Wall (871, -40% damage taken) and Pain Suppression (33206, -40% damage taken), with the Camel as theirs.
     * Steps:
     *  1. a wait; the Camel takes him as its victim, so a fear that ends sends it after him and not home; Frost Nova
     *     and the shield put on the Camel;
     *  2. a Fireball, which the shield takes whole;
     *  3.-4. Fear put on the Camel, and Frost Nova put back on when it does not hold it, so the fear cannot run it out
     *     of range; a Fireball, which the shield takes whole;
     *  5.-6. Fear and Frost Nova taken off the Camel and Hex put on; a Fireball, which the shield takes whole;
     *  7.-8. Hex and the shield taken off the Camel, and Frost Nova, Shield Wall and Pain Suppression put on; a
     *     Fireball, which the two effects reduce to 0.36 of itself;
     *  9. kProcHits times: Frost Nova put on the Camel when it does not hold it, then the Camel's side of the proc call
     *     a Fireball hit of kHitDamage makes (Spell::DoAllEffectOnTarget: Unit::ProcDamageAndSpell with the
     *     taken-negative-spell and damage-taken flags), still under the two effects;
     *  10. Shield Wall and Pain Suppression taken off the Camel; the same kProcHits proc calls.
     *
     * `thresholdDamage`: `absorbed=<122 after its Fireball>/<5782 after its>/<51514 after its>,reducedHit=<122 after
     * the reduced Fireball>,reduced=<calls that took Frost Nova off under the effects>/<calls>,plain=<calls that took
     * it off without them>/<calls>`, 1 for held and 0 for gone. OK when each aura landed, the values read
     * `absorbed=0/0/0,reducedHit=0,reduced=<calls>/<calls>`, the shield fell by each absorbed Fireball while the
     * Camel's health did not fall, the reduced Fireball took at most half of what the shield took of the first one,
     * and plain went on at least one call and stayed on at least one (a chance of kHitDamage / 1975 a call;
     * kHitDamage / 0.36 is over 1975).
     */
    class MageThresholdPreventedDamage : public MageFireProofScenario
    {
    public:
        MageThresholdPreventedDamage() : MageFireProofScenario("mage-threshold-prevented", 1026, "thresholdDamage") {}

    protected:
        std::vector<uint32> Spells() const override { return { kFireball }; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 Watched() const override { return kFrostNova; }
        bool WatchedOnTarget() const override { return true; }
        bool NotesBeforeStep() const override { return true; }

        std::vector<Step> Steps() const override
        {
            return { Wait(kSwingEveryMs), Cast(kFireball, false),
                     Wait(kSwingEveryMs), Cast(kFireball, false),
                     Wait(kSwingEveryMs), Cast(kFireball, false),
                     Wait(kSwingEveryMs), Cast(kFireball, false),
                     Wait(kSwingEveryMs), Wait(kSwingEveryMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 122: 49 fields, fnv 2ec0d9e1
                { 122, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 18 }, { "eff0.EffectBonusCoefficient", 1044750795 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 3 }, { "eff0.EffectRadiusIndex_1", 32 }, { "eff0.EffectRealPointsPerLevel", 1056964608 }, { "eff0.ImplicitTarget_0", 22 }, { "eff0.ImplicitTarget_1", 15 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 26 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMechanic", 7 }, { "eff1.EffectRadiusIndex_1", 32 }, { "eff1.ImplicitTarget_0", 22 }, { "eff1.ImplicitTarget_1", 15 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 5782: 72 fields, fnv 0a17642c
                { 5782, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 7 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 31 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 26 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 51514: 72 fields, fnv e2d49f1b
                { 51514, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 56 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 13321 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 60 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 260 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.EffectMiscValue_0", 754 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 17: 26 fields, fnv bff13de9
                { 17, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 69 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 871: 26 fields, fnv 97c2b77b
                { 871, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 33206: 49 fields, fnv 5f4b6a7c
                { 33206, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 87 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 127 }, { "eff0.EffectSpellClassMask.Flags", 1 }, { "eff0.ImplicitTarget_0", 21 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 235 }, { "eff1.EffectBasePoints", 65 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectSpellClassMask.Flags", 24502834282863 }, { "eff1.ImplicitTarget_0", 21 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            Seen& seen = st.seen[k];
            if (atRead)
            {
                seen.reads["nova"] = target->HasAura(kFrostNova) ? 1 : 0;
                seen.reads["fear"] = target->HasAura(kFear) ? 1 : 0;
                seen.reads["hex"] = target->HasAura(kHex) ? 1 : 0;
                seen.reads["shield"] = ShieldLeft(target);
                seen.reads["health"] = target->GetHealth();
                return;
            }
            seen.reads["healthBefore"] = target->GetHealth();
            seen.reads["shieldBefore"] = ShieldLeft(target);
            if (k == kNovaStep)
            {
                // a creature with no victim runs home when its fear ends, and evades every spell on the way; the
                // silenced Camel takes the mage as its victim, as a creature fighting him has him
                const bool attacking = target->Attack(p, false);
                PutReadyAura(target, kFrostNova, kAuraMs, p);
                PutReadyAura(target, kShield, kAuraMs);
                if (Aura* shield = target->GetAura(kShield, EFFECT_INDEX_0))
                {
                    shield->GetModifier()->m_amount = kShieldAmount;
                }
                seen.reads["put"] = target->HasAura(kFrostNova) ? 1 : 0;
                m_rec.Note(std::string("the Camel takes him as its victim: ") + (attacking ? "it does" : "refused") +
                           "; " + U(kFrostNova) + " put on it: " + HolderOf(target, kFrostNova) + "; " + U(kShield) +
                           " put on it, absorbing " + std::to_string(ShieldLeft(target)));
            }
            else if (k == kFearStep)
            {
                if (!target->HasAura(kFrostNova))
                {
                    PutReadyAura(target, kFrostNova, kAuraMs, p);
                }
                PutReadyAura(target, kFear, kAuraMs, p);
                seen.reads["put"] = target->HasAura(kFear) && target->HasAura(kFrostNova) ? 1 : 0;
                m_rec.Note(U(kFear) + " put on the Camel: " + HolderOf(target, kFear) + "; " + U(kFrostNova) +
                           " on it: " + HolderOf(target, kFrostNova) + "; the shield absorbing " +
                           std::to_string(ShieldLeft(target)));
            }
            else if (k == kHexStep)
            {
                target->RemoveAurasDueToSpell(kFear);
                target->RemoveAurasDueToSpell(kFrostNova);
                PutReadyAura(target, kHex, kAuraMs, p);
                seen.reads["put"] = target->HasAura(kHex) ? 1 : 0;
                m_rec.Note(U(kFear) + " and " + U(kFrostNova) + " taken off the Camel; " + U(kHex) + " put on it: " +
                           HolderOf(target, kHex) + "; the shield absorbing " + std::to_string(ShieldLeft(target)));
            }
            else if (k == kReduceStep)
            {
                target->RemoveAurasDueToSpell(kHex);
                target->RemoveAurasDueToSpell(kShield);
                PutReadyAura(target, kFrostNova, kAuraMs, p);
                PutReadyAura(target, kShieldWall, kAuraMs);
                PutReadyAura(target, kPainSuppression, kAuraMs);
                seen.reads["put"] = target->HasAura(kFrostNova) && target->HasAura(kShieldWall) &&
                                    target->HasAura(kPainSuppression) ? 1 : 0;
                m_rec.Note(U(kHex) + " and " + U(kShield) + " taken off the Camel; " + U(kFrostNova) + ", " +
                           U(kShieldWall) + " and " + U(kPainSuppression) + " put on it: " +
                           HolderOf(target, kFrostNova) + "; " + HolderOf(target, kShieldWall) + "; " +
                           HolderOf(target, kPainSuppression));
            }
            else if (k == kReducedProcStep || k == kPlainProcStep)
            {
                if (k == kPlainProcStep)
                {
                    target->RemoveAurasDueToSpell(kShieldWall);
                    target->RemoveAurasDueToSpell(kPainSuppression);
                }
                SpellEntry const* fireball = sSpellStore.LookupEntry(kFireball);
                uint32 broke = 0;
                for (uint32 i = 0; i < kProcHits; ++i)
                {
                    if (!target->HasAura(kFrostNova))
                    {
                        PutReadyAura(target, kFrostNova, kAuraMs, p);
                    }
                    p->ProcDamageAndSpell(target, PROC_FLAG_NONE,
                                          PROC_FLAG_TAKEN_NEGATIVE_SPELL_HIT | PROC_FLAG_TAKEN_ANY_DAMAGE,
                                          PROC_EX_NORMAL_HIT, kHitDamage, BASE_ATTACK, fireball);
                    broke += target->HasAura(kFrostNova) ? 0 : 1;
                }
                seen.reads["broke"] = broke;
                m_rec.Note(U(kProcHits) + " Fireball hit procs of " + U(kHitDamage) + " at the Camel under " +
                           U(kFrostNova) + (k == kReducedProcStep ? ", " + U(kShieldWall) + " and " +
                           U(kPainSuppression) : std::string(" alone")) + ": they took it off " + U(broke));
            }
        }

        std::string Own(St const& st) const override
        {
            const bool landed = Kept(st, kNovaStep, "put") == 1 && Kept(st, kFearStep, "put") == 1 &&
                                Kept(st, kHexStep, "put") == 1 && Kept(st, kReduceStep, "put") == 1;
            const int64 nova = Kept(st, kNovaHit, "nova");
            const int64 fear = Kept(st, kFearHit, "fear");
            const int64 hex = Kept(st, kHexHit, "hex");
            const int64 reducedHit = Kept(st, kReducedHit, "nova");
            const int64 reduced = Kept(st, kReducedProcStep, "broke");
            const int64 plain = Kept(st, kPlainProcStep, "broke");

            bool shieldTook = true;
            std::string shieldReads;
            const uint32 absorbedHits[] = { kNovaHit, kFearHit, kHexHit };
            for (uint32 k : absorbedHits)
            {
                const int64 before = Kept(st, k, "shieldBefore");
                const int64 after = Kept(st, k, "shield");
                shieldTook = shieldTook && after >= 0 && after < before &&
                             Kept(st, k, "health") >= Kept(st, k, "healthBefore");
                shieldReads += (shieldReads.empty() ? "" : ", ") + std::to_string(before) + " to " +
                               std::to_string(after);
            }
            const int64 firstTake = Kept(st, kNovaHit, "shieldBefore") - Kept(st, kNovaHit, "shield");
            const int64 reducedTake = Kept(st, kReducedHit, "healthBefore") - Kept(st, kReducedHit, "health");
            const bool effectsTook = reducedTake > 0 && reducedTake * 2 <= firstTake;

            const bool ok = landed && nova == 0 && fear == 0 && hex == 0 && shieldTook && reducedHit == 0 &&
                            effectsTook && reduced == int64(kProcHits) && plain >= 1 && plain < int64(kProcHits);
            const std::string value = "absorbed=" + std::to_string(nova) + "/" + std::to_string(fear) + "/" +
                                      std::to_string(hex) + ",reducedHit=" + std::to_string(reducedHit) +
                                      ",reduced=" + std::to_string(reduced) + "/" + U(kProcHits) +
                                      ",plain=" + std::to_string(plain) + "/" + U(kProcHits);
            const std::string detail =
                std::string("each aura ") + (landed ? "landed" : "did not all land") + "; the shield read " +
                shieldReads + " over the three absorbed Fireballs, the Camel's health not falling: " +
                (shieldTook ? "yes" : "no") + "; the reduced Fireball took " + std::to_string(reducedTake) +
                " of the Camel's health, the shield took " + std::to_string(firstTake) + " of the first; wanted " +
                "absorbed=0/0/0,reducedHit=0,reduced=" + U(kProcHits) + "/" + U(kProcHits) + ", the shield taking " +
                "each Fireball whole, the reduced one at most half the first, and plain both gone and kept";
            return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kFrostNova = 122;
        static constexpr uint32 kFear = 5782;
        static constexpr uint32 kHex = 51514;
        static constexpr uint32 kShield = 17;
        static constexpr uint32 kShieldWall = 871;
        static constexpr uint32 kPainSuppression = 33206;
        static constexpr int32 kShieldAmount = 100000;
        static constexpr int32 kAuraMs = 60000;
        static constexpr uint32 kProcHits = 20;
        static constexpr uint32 kHitDamage = 720;
        static constexpr uint32 kNovaStep = 0;
        static constexpr uint32 kNovaHit = 1;
        static constexpr uint32 kFearStep = 2;
        static constexpr uint32 kFearHit = 3;
        static constexpr uint32 kHexStep = 4;
        static constexpr uint32 kHexHit = 5;
        static constexpr uint32 kReduceStep = 6;
        static constexpr uint32 kReducedHit = 7;
        static constexpr uint32 kReducedProcStep = 8;
        static constexpr uint32 kPlainProcStep = 9;

        /// What the Camel's Power Word: Shield has left to absorb; -1 when it holds none.
        static int64 ShieldLeft(Unit* unit)
        {
            Aura* shield = unit->GetAura(kShield, EFFECT_INDEX_0);
            return shield ? int64(shield->GetModifier()->m_amount) : -1;
        }
    };

    /**
     * S941 `mage-fire-share-before-taken`: Ignite and Glyph of Frostfire's DoT take their share of a hit before the
     * target's damage-taken effects, which each tick then takes once. A Fire mage staged as in 966 (Burning Soul 3,
     * Improved Fire Blast 2, Ignite 3, kSpellPower spell power, no mastery) and 984 (Glyph of Frostfire, Frostfire Bolt
     * learned), with Precision on him before each cast. The Camel holds Curse of the Elements (1490) from its spawn:
     * its effect 1 is a damage-taken aura of +8% over the six schools. Steps: a Fireball that must crit, kFireballEveryMs
     * before a Frostfire Bolt that must not (the Ignite is out by then), then the curse taken off the Camel, then the
     * same Fireball and bolt again with no damage-taken aura on it. Each Fireball's crit puts Ignite on the Camel (two
     * ticks) and each bolt's glyph DoT ticks four times.
     *
     * `share`: `ignite=<the two Ignite ticks on the cursed Camel>+<the two on the bare one>,bolt=<the four glyph ticks
     * on the cursed Camel>+<the four on the bare one>`, each tick group `/`-joined. With t the Camel's damage-taken
     * factor (1.08 cursed, 1 bare), Ignite's tick is int(int(D / t + 0.5) x 40 / 100) / 2 x t and the glyph's is
     * int(int(h / t + 0.5) x <61205's effect 2> / 100) / <ticks> x t, each truncated as the engine truncates, for D the
     * Fireball's hit and h the bolt's hit dealt and absorbed. OK when both Fireballs crit, no bolt or tick crits, each
     * group has its tick count and every tick is that value. BUG with the values in the detail otherwise.
     */
    class MageFireShareBeforeTaken : public MageProcScenario
    {
    public:
        MageFireShareBeforeTaken() : MageProcScenario("mage-fire-share-before-taken", 941, "share") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostfireSlot }; }
        int32 SpellPower() const override { return kSpellPower; }

        std::vector<Step> Steps() const override
        {
            Step fireball = Cast(133, true);
            fireball.everyMs = kFireballEveryMs;
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kLastStepMs;
            return { fireball, bolt, Removal(kCurse, AURA_REMOVE_BY_DEFAULT, kRemovalEveryMs), fireball, bolt };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 1490: 72 fields
                { 1490, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 22 }, { "eff0.EffectBasePoints", -45 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 87 }, { "eff1.EffectBasePoints", 8 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 226 }, { "eff2.EffectAuraPeriod", 2000 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
            };
        }

        uint32 Watched() const override { return kFrostfireBolt; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 TargetAura() const override { return kCurse; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 CastReadMs() const override { return kBoltReadMs; }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* /*p*/, Creature* target) override
        {
            if (!atRead)
            {
                return;
            }
            Aura const* curse = target->GetAura(kCurse, EFFECT_INDEX_1);
            st.seen[k].reads["curse"] = curse ? curse->GetModifier()->m_amount : kNone;
            m_rec.Note("1490 on the Camel: " + HolderOf(target, kCurse) + "; its damage-taken effect " +
                       (curse ? std::to_string(curse->GetModifier()->m_amount) : std::string("absent")));
        }

        std::string Own(St const& st) const override
        {
            // the Fireball hits and the Ignite ticks after each, the bolt hits and the glyph ticks after each, in the
            // order the record holds them
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> fireballs;
            std::vector<uint32> bolts;
            std::vector<std::vector<uint32> > igniteTicks;
            std::vector<std::vector<uint32> > boltTicks;
            bool fireballCrit = true;
            bool boltCrit = false;
            bool tickCrit = false;
            std::vector<Recorder::Seen> const& packets = m_rec.Packets();
            for (size_t i = 0; i < packets.size(); ++i)
            {
                Recorder::Seen const& packet = packets[i];
                const bool hit = packet.opcode == SMSG_SPELLNONMELEEDAMAGELOG;
                if ((!hit && packet.opcode != SMSG_PERIODICAURALOG) || packet.payload.empty())
                {
                    continue;
                }
                const std::vector<std::string> fields = Trace::CombatLogFields(packet.opcode, &packet.payload[0], packet.payload.size(), roles);
                for (size_t f = 0; f < fields.size(); ++f)
                {
                    uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                    const int parsed = hit ? sscanf(fields[f].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted)
                                           : sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted);
                    if (parsed != 5)
                    {
                        continue;
                    }
                    if (hit && spell == 133)
                    {
                        fireballs.push_back(amount + absorbed);
                        fireballCrit = fireballCrit && crit != 0;
                        igniteTicks.push_back(std::vector<uint32>());
                    }
                    else if (hit && spell == kFrostfireBolt)
                    {
                        bolts.push_back(amount + absorbed);
                        boltCrit = boltCrit || crit != 0;
                        boltTicks.push_back(std::vector<uint32>());
                    }
                    else if (!hit && spell == kIgniteDot && !igniteTicks.empty())
                    {
                        igniteTicks.back().push_back(amount);
                        tickCrit = tickCrit || crit != 0;
                    }
                    else if (!hit && spell == kFrostfireBolt && !boltTicks.empty())
                    {
                        boltTicks.back().push_back(amount);
                        tickCrit = tickCrit || crit != 0;
                    }
                }
            }

            // the glyph's percent from its row, Ignite's tick count from its row, and the glyph DoT's tick count over
            // its full duration at the bolts' reads
            SpellEntry const* glyph = sSpellStore.LookupEntry(kGlyphOfFrostfire);
            const int32 percent = glyph ? glyph->CalculateSimpleValue(EFFECT_INDEX_2) : 0;
            SpellEntry const* dot = sSpellStore.LookupEntry(kIgniteDot);
            SpellEffectEntry const* dotEffect = dot ? dot->GetSpellEffect(EFFECT_INDEX_0) : NULL;
            const int32 duration = GetSpellDuration(dot);
            const uint32 period = dotEffect ? dotEffect->EffectAuraPeriod : 0;
            const uint32 igniteCount = period && duration > 0 ? uint32(duration) / period : 0;
            const uint32 boltCount = BoltTicksAt(st, kFirstBolt);

            bool ok = fireballs.size() == 2 && bolts.size() == 2 && fireballCrit && !boltCrit && !tickCrit && percent > 0 &&
                      igniteCount > 0 && boltCount > 0 && BoltTicksAt(st, kSecondBolt) == boltCount;
            const int64 curse = Kept(st, kFirstFireball, "curse");
            ok = ok && curse > 0 && Kept(st, kRemoval, "curse") == kNone;

            const bool counted = fireballs.size() == 2 && bolts.size() == 2 && igniteTicks.size() == 2 && boltTicks.size() == 2;
            std::string igniteText, boltText, detail;
            for (uint32 phase = 0; phase < 2 && counted && igniteCount > 0 && boltCount > 0; ++phase)
            {
                const float taken = phase == 0 ? (100.0f + float(curse)) / 100.0f : 1.0f;
                const uint32 igniteBase = uint32(float(fireballs[phase]) / taken + 0.5f);
                const int32 igniteShare = int32(float(igniteBase) * kIgnitePercent / 100) / int32(igniteCount);
                const uint32 igniteWant = uint32(float(igniteShare) * taken);
                const uint32 boltBase = uint32(float(bolts[phase]) / taken + 0.5f);
                const int32 boltShare = int32(float(boltBase) * uint32(percent) / 100) / int32(boltCount);
                const uint32 boltWant = uint32(float(boltShare) * taken);
                ok = ok && igniteTicks[phase].size() == igniteCount && boltTicks[phase].size() == boltCount;
                igniteText += (phase ? "+" : "") + TickList(igniteTicks[phase]);
                boltText += (phase ? "+" : "") + TickList(boltTicks[phase]);
                for (uint32 tick : igniteTicks[phase])
                {
                    ok = ok && tick == igniteWant;
                }
                for (uint32 tick : boltTicks[phase])
                {
                    ok = ok && tick == boltWant;
                }
                detail += std::string(phase ? "; bare Camel: " : "cursed Camel: ") + "Fireball " + U(fireballs[phase]) + " (share base " +
                          U(igniteBase) + ", tick " + U(igniteWant) + " each), Frostfire Bolt " + U(bolts[phase]) + " (share base " +
                          U(boltBase) + ", tick " + U(boltWant) + " each)";
            }
            if (!ok && detail.empty())
            {
                detail = "fireballs " + U(fireballs.size()) + ", bolts " + U(bolts.size()) + ", curse " + std::to_string(curse) +
                         ", fireball crit " + (fireballCrit ? "yes" : "no") + ", bolt crit " + (boltCrit ? "yes" : "no") +
                         ", tick crit " + (tickCrit ? "yes" : "no") + ", ignite ticks per Ignite " + U(igniteCount) +
                         ", bolt ticks per DoT " + U(boltCount);
            }
            const std::string value = "ignite=" + (igniteText.empty() ? std::string("none") : igniteText) + ",bolt=" +
                                      (boltText.empty() ? std::string("none") : boltText);
            return value + (ok ? "(OK: " : "(BUG: ") + detail + ")";
        }

    private:
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfire = 61205;
        static constexpr uint32 kGlyphOfFrostfireSlot = 591;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kCurse = 1490;
        static constexpr uint32 kPrecision = 13843;
        static constexpr uint32 kIgnitePercent = 40;
        static constexpr int32 kSpellPower = 1000;
        static constexpr uint32 kFireballEveryMs = 6000;
        static constexpr uint32 kRemovalEveryMs = 1000;
        static constexpr uint32 kLastStepMs = 15500;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kFirstFireball = 0;
        static constexpr uint32 kFirstBolt = 1;
        static constexpr uint32 kRemoval = 2;
        static constexpr uint32 kSecondBolt = 4;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }

        /// The tick count of the bolt's DoT at the read of step k: its full duration over its period; 0 for none.
        static uint32 BoltTicksAt(St const& st, uint32 k)
        {
            if (k >= st.seen.size())
            {
                return 0;
            }
            Seen const& seen = st.seen[k];
            return seen.periodicRead && seen.period > 0 ? uint32(seen.maxDuration / seen.period) : 0;
        }

        /// The amounts `/`-joined; `none` for none.
        static std::string TickList(std::vector<uint32> const& amounts)
        {
            std::string out;
            for (size_t i = 0; i < amounts.size(); ++i)
            {
                out += (i ? "/" : "") + U(amounts[i]);
            }
            return out.empty() ? std::string("none") : out;
        }
    };

    /**
     * S944 `mage-fire-flame-orb-owner-hit`: 971's mage and orb with the Camel at level 88 holding no Hit Me and the mage
     * holding Don't Miss! (73411, +100 spell hit and +100 hit), so he is at his spell hit cap. One step: Flame Orb under
     * the crit aura, kStepMs to the end. S947 `mage-fire-flame-orb-owner-hit-uncapped` is the same without
     * Don't Miss! and with kHitRating (820) of spell hit rating put on him, so his hit is above 0 and under the cap.
     *
     * `orbHit`: `goes=<SPELL_GO of the orb's hit>,misses=<of them listing the Camel as missed>,hits=<its damage logs on
     * the Camel>,owner=<his spell hit field>,share=<the spell hit of Combat::OwnerShareOf(orb)>/<its expertise>`; OK
     * when there are kOrbHits goes and every one is a hit or a miss, the owner's field is 100 (S947: above 0 and under
     * the spell cap) and the share is the spell cap of 17 with 650 expertise and no hit missed (S947: the owner's hit
     * in proportion, and misses are allowed): the orb rolls with its summoner's hit as of its summon.
     */
    class MageFireFlameOrbOwnerHit : public MageFireFlameOrb
    {
    public:
        MageFireFlameOrbOwnerHit() : MageFireFlameOrbOwnerHit("mage-fire-flame-orb-owner-hit", 944, true) {}
        MageFireFlameOrbOwnerHit(char const* name, int order, bool capped) : MageFireFlameOrb(name, order, "orbHit"), m_capped(capped) {}

    protected:
        std::vector<Step> Steps() const override
        {
            std::vector<Step> steps;
            std::vector<Step> orb = MageFireFlameOrb::Steps();
            steps.insert(steps.end(), orb.begin(), orb.end());
            return steps;
        }

        std::vector<SpellPrint> Prints() const override
        {
            std::vector<SpellPrint> prints = MageFireFlameOrb::Prints();
            prints.push_back(
                { 73411, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 100 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } }
            );
            return prints;
        }

        uint32 TargetAura() const override { return 0; }
        uint32 TargetLevel() const override { return kTargetLevel; }
        uint32 CasterAura() const override { return m_capped ? kDontMiss : 0; }
        bool CasterAuraPut() const override { return true; }

        void AfterStage(Player& p) override
        {
            if (!m_capped)
            {
                p.ApplyRatingMod(CR_HIT_SPELL, kHitRating, true);
            }
        }

        std::string Own(St const& st) const override
        {
            Trace::Roles const& roles = m_rec.Roles();
            const uint64 camel = st.target.GetRawValue();
            uint32 goes = 0, missed = 0, hits = 0;
            const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                for (const char* suffix : suffixes)
                {
                    const std::string window = "step" + U(k + 1) + suffix;
                    std::vector<Recorder::Seen const*> gos = m_rec.SeenIn(window, SMSG_SPELL_GO);
                    for (size_t i = 0; i < gos.size(); ++i)
                    {
                        std::string go;
                        uint32 spell = 0;
                        if (gos[i]->payload.empty() || !Trace::DecodeSpellCast(true, &gos[i]->payload[0], gos[i]->payload.size(), roles, go) ||
                            sscanf(go.c_str(), "spell=%u", &spell) != 1 || spell != kOrbHit)
                        {
                            continue;
                        }
                        ++goes;
                        missed += go.find(" hit=[] miss=[target:") != std::string::npos ? 1 : 0;
                    }
                    std::vector<Recorder::Seen const*> logs = m_rec.SeenIn(window, SMSG_SPELLNONMELEEDAMAGELOG);
                    for (size_t i = 0; i < logs.size(); ++i)
                    {
                        Trace::SpellDamageLog log;
                        if (!logs[i]->payload.empty() && Trace::ReadSpellDamageLog(&logs[i]->payload[0], logs[i]->payload.size(), log) &&
                            log.spell == kOrbHit && log.target == camel)
                        {
                            ++hits;
                        }
                    }
                }
            }
            Player* p = sPlayerRegistry.Find(st.player);
            const float owner = p ? p->m_modSpellHitChance : 0.0f;
            const Combat::HitCaps caps = Combat::BossHitCaps();
            const float wantShare = m_capped ? caps.spell : Combat::InheritedHit(owner, caps.spell, caps.spell);
            const int32 wantExpertise = m_capped ? 650 : Combat::InheritedExpertise(owner, caps.spell);
            char buf[160];
            snprintf(buf, sizeof(buf), "goes=%u,misses=%u,hits=%u,owner=%.0f,share=%.2f/%d", goes, missed, hits, owner, m_orbShare.spell,
                     m_orbShare.expertise);
            const bool ownerOk = m_capped ? owner == 100.0f : (owner > 0.0f && owner < caps.spell);
            const bool shareOk = std::fabs(m_orbShare.spell - wantShare) < 0.005f && m_orbShare.expertise == wantExpertise;
            const bool ok = goes == kOrbHits && (!m_capped || missed == 0) && hits + missed == kOrbHits && ownerOk && shareOk;
            return (ok ? "OK(" : "BUG(") + std::string(buf) + ")";
        }

    private:
        static constexpr uint32 kOrbHit = 82739;
        static constexpr uint32 kDontMiss = 73411;
        static constexpr uint32 kTargetLevel = 88;
        static constexpr int32 kHitRating = 820;
        static constexpr uint32 kOrbHits = 15;

        bool m_capped;
    };

    /**
     * S945 `mage-fire-combustion-at-the-cast`: Combustion's DoT (83853) builds its Living Bomb and Pyroblast shares from
     * the mage's spell power and mastery at the cast, over the DoTs' own unhasted period, with Flashburn once in each
     * share and the target's damage-taken effects once in the tick. A Fire mage staged as 955 stages him, with Living
     * Bomb and the Mastery passive (86473) learned, kMasteryRating mastery rating, kHasteRating haste rating (20% cast
     * haste, so Pyroblast's and Living Bomb's DoTs tick every 2.5 s and 83853 every 0.83 s) and kSpellPower spell power.
     * The Camel holds Curse of the Elements (1490) from its spawn: +8% damage taken. Steps:
     *  1. Pyroblast, which must crit: its DoT and Ignite (12654) go on the Camel;
     *  2. Living Bomb (44457) on the Camel;
     *  3. Flame of the Seer (52281) cast on him at kSpellPowerGain: that much more spell power, after both DoTs went up;
     *  4. Combustion (11129), which must not crit: the Ignite, Pyroblast and Living Bomb ticks the Camel holds are read
     *     as it casts, and the two DoTs' tick as the caster's done damage of a DoT gives it now (SpellDamageBonusDone,
     *     on the amount before the caster's bonus the aura holds);
     *  5. Pyroblast and 6. Living Bomb again, not crit: the DoTs' amounts a fresh cast holds at the new spell power,
     *     which are the two tick values step 4 read.
     *
     * `atTheCast`: `tick=<the first 83853 tick>,ticks=<how many>,want=<the band>,sp=<spell power before>/<after>,
     * fresh=<Living Bomb>/<Pyroblast>`. With I the Ignite tick the aura holds, L and P the Living Bomb and Pyroblast
     * ticks at the new spell power and mastery, each as the aura of a fresh cast holds it, the tick before the Camel's
     * damage-taken effects is I / 2 + L / 3 + P / 3, one Flashburn in each term, and the band is that value, less the
     * three the integer steps may drop and plus one, each through SpellDamageBonusTaken on the Camel, which takes the
     * curse once. OK when the staged spell power rose by at least kSpellPowerGain less one, the fresh amounts are the
     * values step 4 read, every 83853 tick is the first, it is in the band and there are more than ten of them (haste
     * adds ticks and does not change a tick), and no tick crit. BUG with the values in the detail otherwise.
     */
    class MageFireCombustionAtTheCast : public MageProcScenario
    {
    public:
        MageFireCombustionAtTheCast() : MageProcScenario("mage-fire-combustion-at-the-cast", 945, "atTheCast") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 0 }, { 10561, 0 } };
        }
        std::vector<uint32> Spells() const override { return { kLivingBomb, kMasteryPassive }; }
        int32 SpellPower() const override { return kSpellPower; }
        int32 HasteRating() const override { return kHasteRating; }
        int32 MasteryRating() const override { return kMasteryRating; }
        uint32 CasterAura() const override { return kPrecision; }
        uint32 TargetAura() const override { return kCurse; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kCombustionDot; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }

        std::vector<Step> Steps() const override
        {
            Step pyroblast = Cast(kPyroblast, true);
            pyroblast.everyMs = kBombAfterMs;
            Step combustion = Cast(kCombustion, false);
            combustion.everyMs = kRecastAfterMs;
            Step pyroblastAgain = Cast(kPyroblast, false);
            pyroblastAgain.everyMs = kBombAgainAfterMs;
            return { pyroblast, AuraCast(kLivingBomb, kPowerAfterMs), Wait(kCombustionAfterMs), combustion, pyroblastAgain,
                     AuraCast(kLivingBomb, kLastStepMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 76595: 49 fields, fnv ab04106a
                { 76595, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBonusCoefficient", 1077097267 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 580542277877760 }, { "eff0.EffectSpellClassMask.Flags2", 1024 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 280 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 52281: 26 fields, fnv e7106a10
                { 52281, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 13 }, { "eff0.EffectBasePoints", 400 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 1490: 72 fields, fnv 4bc3085d
                { 1490, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 22 }, { "eff0.EffectBasePoints", -45 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 126 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 87 }, { "eff1.EffectBasePoints", 8 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 226 }, { "eff2.EffectAuraPeriod", 2000 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (k == kPowerStep && !atRead)
            {
                const int32 before = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
                int32 points = kSpellPowerGain;
                p->CastCustomSpell(p, kSpellPowerAura, &points, NULL, NULL, true);
                const int32 after = p->SpellBaseDamageBonusDone(SPELL_SCHOOL_MASK_FIRE);
                st.seen[k].reads["spBefore"] = before;
                st.seen[k].reads["spAfter"] = after;
                m_rec.Note("his " + U(kSpellPowerAura) + " cast on him at " + std::to_string(kSpellPowerGain) + "; fire spell power " +
                           std::to_string(before) + " -> " + std::to_string(after));
                return;
            }
            if (k == kCombustionStep && !atRead)
            {
                Aura const* ignite = target->GetAura(kIgniteDot, EFFECT_INDEX_0);
                Aura const* bomb = target->GetAura(kLivingBomb, EFFECT_INDEX_0);
                Aura const* pyroblast = target->GetAura(kPyroblast, EFFECT_INDEX_1);
                SpellEntry const* bombSpell = sSpellStore.LookupEntry(kLivingBomb);
                SpellEntry const* pyroblastSpell = sSpellStore.LookupEntry(kPyroblast);
                SpellEntry const* combustionSpell = sSpellStore.LookupEntry(kCombustionDot);
                if (!ignite || !bomb || !pyroblast || !bombSpell || !pyroblastSpell || !combustionSpell)
                {
                    m_rec.Note(std::string("a DoT is missing at the cast: Ignite ") + (ignite ? "held" : "absent") + ", Living Bomb " +
                               (bomb ? "held" : "absent") + ", Pyroblast " + (pyroblast ? "held" : "absent"));
                    return;
                }
                const uint32 bombDone = p->SpellDamageBonusDone(target, bombSpell, uint32(bomb->GetTickAmountBeforeDone()), DOT,
                                                                 bomb->GetStackAmount());
                const uint32 pyroblastDone = p->SpellDamageBonusDone(target, pyroblastSpell, uint32(pyroblast->GetTickAmountBeforeDone()), DOT,
                                                                     pyroblast->GetStackAmount());
                const uint32 igniteTick = uint32(ignite->GetModifier()->m_amount);
                const double sum = double(igniteTick) * 1000.0 / double(kIgnitePeriod) + double(bombDone) * 1000.0 / double(kShareDbcPeriod) +
                                   double(pyroblastDone) * 1000.0 / double(kShareDbcPeriod);
                const uint32 before = uint32(sum);
                const uint32 low = before > kRoundingLoss ? before - kRoundingLoss : 0;
                Seen& seen = st.seen[k];
                seen.reads["ignite"] = igniteTick;
                seen.reads["bomb"] = bombDone;
                seen.reads["pyroblast"] = pyroblastDone;
                seen.reads["bombBefore"] = bomb->GetTickAmountBeforeDone();
                seen.reads["pyroblastBefore"] = pyroblast->GetTickAmountBeforeDone();
                seen.reads["beforeTaken"] = before;
                seen.reads["wantLow"] = target->SpellDamageBonusTaken(p, combustionSpell, low, DOT, 1);
                seen.reads["wantHigh"] = target->SpellDamageBonusTaken(p, combustionSpell, before + 1, DOT, 1);
                m_rec.Note("at the cast: Ignite's tick " + U(igniteTick) + ", Living Bomb " + U(uint32(bomb->GetTickAmountBeforeDone())) + " -> " +
                           U(bombDone) + ", Pyroblast " + U(uint32(pyroblast->GetTickAmountBeforeDone())) + " -> " + U(pyroblastDone) +
                           "; before the Camel's damage-taken effects " + U(before) + ", taken " + U(uint32(seen.reads["wantLow"])) + ".." +
                           U(uint32(seen.reads["wantHigh"])));
                return;
            }
            if (atRead && (k == kPyroblastAgainStep || k == kBombAgainStep))
            {
                Aura const* fresh = k == kBombAgainStep ? target->GetAura(kLivingBomb, EFFECT_INDEX_0) : target->GetAura(kPyroblast, EFFECT_INDEX_1);
                st.seen[k].reads["fresh"] = fresh ? fresh->GetModifier()->m_amount : kNone;
                m_rec.Note(std::string("the fresh ") + (k == kBombAgainStep ? "Living Bomb" : "Pyroblast") + " DoT holds " +
                           (fresh ? std::to_string(fresh->GetModifier()->m_amount) : std::string("nothing")));
            }
        }

        std::string Own(St const& st) const override
        {
            std::vector<std::pair<uint32, bool> > ticks;
            Trace::Roles const& roles = m_rec.Roles();
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                                spell == kCombustionDot)
                            {
                                ticks.push_back(std::make_pair(amount, crit != 0));
                            }
                        }
                    }
                }
            }

            const int64 spBefore = Kept(st, kPowerStep, "spBefore");
            const int64 spAfter = Kept(st, kPowerStep, "spAfter");
            const int64 low = Kept(st, kCombustionStep, "wantLow");
            const int64 high = Kept(st, kCombustionStep, "wantHigh");
            const int64 bomb = Kept(st, kCombustionStep, "bomb");
            const int64 pyroblast = Kept(st, kCombustionStep, "pyroblast");
            const int64 freshBomb = Kept(st, kBombAgainStep, "fresh");
            const int64 freshPyroblast = Kept(st, kPyroblastAgainStep, "fresh");
            const uint32 first = ticks.empty() ? 0 : ticks[0].first;

            bool ok = st.steps.size() == 6 && st.seen[kPyroblastStep].logRead && st.seen[kPyroblastStep].crit && spBefore >= 0 &&
                      spAfter - spBefore >= kSpellPowerGain - 1 && low >= 0 && high >= low && ticks.size() > 10 &&
                      int64(first) >= low && int64(first) <= high && freshBomb == bomb && freshPyroblast == pyroblast;
            std::string tickText;
            for (size_t t = 0; t < ticks.size(); ++t)
            {
                ok = ok && ticks[t].first == first && !ticks[t].second;
                tickText += (t ? " " : "") + U(ticks[t].first) + (ticks[t].second ? "c" : "");
            }

            Seen const& combustionStep = st.seen.size() > kCombustionStep ? st.seen[kCombustionStep] : st.seen[0];
            const std::string period = combustionStep.periodicRead
                                       ? "period " + std::to_string(combustionStep.period) + ", duration " + std::to_string(combustionStep.duration) +
                                         "/" + std::to_string(combustionStep.maxDuration)
                                       : std::string("no periodic effect");
            const std::string value = "tick=" + (ticks.empty() ? std::string("none") : U(first)) + ",ticks=" + U(ticks.size()) + ",want=" +
                                      std::to_string(low) + ".." + std::to_string(high) + ",sp=" + std::to_string(spBefore) + "/" +
                                      std::to_string(spAfter) + ",fresh=" + std::to_string(freshBomb) + "/" + std::to_string(freshPyroblast);
            char buf[1280];
            snprintf(buf, sizeof(buf),
                     "83853 ticks [%s]; Ignite's tick %lld, Living Bomb at the new spell power %lld (its amount before the caster's bonus %lld), "
                     "Pyroblast %lld (%lld): %lld / 2 + %lld / 3 + %lld / 3 = %lld before the Camel's damage-taken effects, taken %lld..%lld; "
                     "the fresh casts' DoTs hold %lld and %lld, wanted %lld and %lld; 83853 at the read of step 4: %s; Pyroblast %s",
                     tickText.c_str(), (long long)Kept(st, kCombustionStep, "ignite"), (long long)bomb,
                     (long long)Kept(st, kCombustionStep, "bombBefore"), (long long)pyroblast,
                     (long long)Kept(st, kCombustionStep, "pyroblastBefore"), (long long)Kept(st, kCombustionStep, "ignite"), (long long)bomb,
                     (long long)pyroblast, (long long)Kept(st, kCombustionStep, "beforeTaken"), (long long)low, (long long)high,
                     (long long)freshBomb, (long long)freshPyroblast, (long long)bomb, (long long)pyroblast, period.c_str(),
                     st.seen[kPyroblastStep].logRead ? (st.seen[kPyroblastStep].crit ? "crit" : "no crit") : "unread");
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kCombustion = 11129;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kLivingBomb = 44457;
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kSpellPowerAura = 52281;
        static constexpr uint32 kCurse = 1490;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr int32 kSpellPowerGain = 1500;
        static constexpr int32 kHasteRating = 2561;
        static constexpr int32 kMasteryRating = 1000;
        static constexpr uint32 kIgnitePeriod = 2000;
        static constexpr uint32 kShareDbcPeriod = 3000;
        static constexpr uint32 kRoundingLoss = 3;
        static constexpr uint32 kBombAfterMs = 3700;
        static constexpr uint32 kPowerAfterMs = 1000;
        static constexpr uint32 kCombustionAfterMs = 1000;
        static constexpr uint32 kRecastAfterMs = 3700;
        static constexpr uint32 kBombAgainAfterMs = 4000;
        static constexpr uint32 kLastStepMs = 4500;
        static constexpr uint32 kPyroblastStep = 0;
        static constexpr uint32 kPowerStep = 2;
        static constexpr uint32 kCombustionStep = 3;
        static constexpr uint32 kPyroblastAgainStep = 4;
        static constexpr uint32 kBombAgainStep = 5;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }
    };

    /**
     * S949 `mage-fire-combustion-mastery-at-the-cast`: Combustion's DoT (83853) takes its Ignite and Glyph of Frostfire
     * parts at the mastery the mage has when he casts it, not at the mastery he had when the Ignite and the Frostfire
     * Bolt landed. A Fire mage staged as 945 stages him, with the Mastery passive (86473) and Glyph of Frostfire
     * (591, its aura 61205) and kMasteryRating mastery rating, kSpellPower spell power and no haste, on a bare Camel.
     * Steps:
     *  1-3. Frostfire Bolt (44614), three times, not crit: the glyph's DoT on the Camel at three stacks;
     *  4. Pyroblast (11366), which must crit: its DoT and Ignite (12654) go on the Camel;
     *  5. nothing, but kMasteryGain more mastery rating is added to him by call as an item's rating adds it, after both
     *     hits landed;
     *  6. Combustion (11129), which must not crit: the Ignite, Pyroblast and Frostfire DoTs the Camel holds are read
     *     as it casts;
     *  7. nothing, to read the 83853 ticks.
     *
     * `masteryAtTheCast`: `tick=<the first 83853 tick>,ticks=<how many>,want=<the band>,flashburn=<before>/<after>,
     * moves=<Ignite>/<glyph>`. With F the Flashburn on 83853 at the cast, I the Ignite tick before its mastery (the
     * amount the aura holds before the done step) and G the glyph tick before its mastery, over the aura's period g,
     * and P the Pyroblast tick as its caster's done damage of a DoT gives it now over 3000 ms, the sum a second is
     * I / 2 + P / 3 / F + G / g (Ignite and the glyph at the cast's mastery once, Pyroblast's built at it and divided
     * by it), and the tick is that sum taken through 83853's done damage (SpellDamageBonusDone, which gives F once) and
     * the Camel's damage-taken effects, within one. `moves` are the seconds' damage the Ignite and the glyph parts move
     * between the two masteries: I x (F - F0) / 2 and G x (F - F0) / g, so a part left at the mastery of its hit shows.
     * F0 is the Flashburn at the hits. OK when mastery rose, I x F0 and G x F0 are each the tick its aura stores within
     * one (so I and G hold no mastery), both moves are at least kMinMove, every 83853 tick is the first and in the
     * band, there are at least nine of them and none crit. BUG with the values in the detail otherwise.
     */
    class MageFireCombustionMasteryAtTheCast : public MageProcScenario
    {
    public:
        MageFireCombustionMasteryAtTheCast()
            : MageProcScenario("mage-fire-combustion-mastery-at-the-cast", 949, "masteryAtTheCast") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 }, { 11434, 2 }, { 10555, 1 }, { 10573, 0 }, { 10547, 0 }, { 10561, 0 } };
        }
        std::vector<uint32> Spells() const override { return { kFrostfireBolt, kMasteryPassive }; }
        std::vector<uint32> Glyphs() const override { return { kGlyphOfFrostfireSlot }; }
        int32 SpellPower() const override { return kSpellPower; }
        int32 MasteryRating() const override { return kMasteryRating; }
        uint32 CasterAura() const override { return kPrecision; }
        int32 NonCritAuraPoints() const override { return -100; }
        uint32 Watched() const override { return kCombustionDot; }
        bool WatchedOnTarget() const override { return true; }
        bool ReadsPeriodic() const override { return true; }
        uint32 CastReadMs() const override { return kBoltReadMs; }

        std::vector<Step> Steps() const override
        {
            Step bolt = Cast(kFrostfireBolt, false);
            bolt.everyMs = kBoltEveryMs;
            Step pyroblast = Cast(kPyroblast, true);
            pyroblast.everyMs = kPyroblastEveryMs;
            Step combustion = Cast(kCombustion, false);
            combustion.everyMs = kCombustionEveryMs;
            return { bolt, bolt, bolt, pyroblast, Wait(kGainWaitMs), combustion, Wait(kLastStepMs) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return {
                // 44614: 72 fields, fnv 0aade896
                { 44614, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 33 }, { "eff0.EffectBasePoints", -40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMechanic", 11 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 628 }, { "eff1.EffectBonusCoefficient", 1064967340 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 103 }, { "eff1.EffectRealPointsPerLevel", 1081711002 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 3 }, { "eff2.EffectAuraPeriod", 3000 }, { "eff2.EffectBasePoints", 1 }, { "eff2.EffectBonusCoefficient", 1005596799 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 6 }, { "eff2.EffectIndex", 2 } } },
                // 61205: 72 fields, fnv a41173db
                { 61205, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 15 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17592186044416 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 3000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 1 }, { "eff1.EffectSpellClassMask.Flags", 17592186044416 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 1 }, { "eff2.Effect", 6 }, { "eff2.EffectAura", 4 }, { "eff2.EffectBasePoints", 3 }, { "eff2.EffectChainAmplitude", 1065353216 }, { "eff2.ImplicitTarget_0", 1 }, { "eff2.EffectIndex", 2 } } },
                // 11366: 49 fields, fnv 7ef90b4a
                { 11366, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 11129: 49 fields, fnv 9ccc32ac
                { 11129, { { "eff0.present", 1 }, { "eff0.Effect", 77 }, { "eff0.EffectBasePoints", 100 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 17867076534295 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 2 }, { "eff1.EffectBasePoints", 23 }, { "eff1.EffectBonusCoefficient", 1054582243 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectDieSides", 9 }, { "eff1.EffectRealPointsPerLevel", 1058642330 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 83853: 26 fields, fnv 87a2a19f
                { 83853, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 25 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44445: 49 fields, fnv effa745d
                { 44445, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 33 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 34359738368 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 25 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 31642: 26 fields, fnv bac9b262
                { 31642, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 4194325 }, { "eff0.EffectTriggerSpell", 18350 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                { 18350, { { "absent", 1 } } },
                // 79684: 26 fields, fnv 8068d7f8
                { 79684, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 79683 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 79683: 26 fields, fnv 0d0f7f9c
                { 79683, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 1 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71761: 26 fields, fnv 6b2bec1a
                { 71761, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 42 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectTriggerSpell", 71757 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 71757: 26 fields, fnv 73f2d232
                { 71757, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 1468 }, { "eff0.EffectBonusCoefficient", 1073985094 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 273 }, { "eff0.EffectRealPointsPerLevel", 1110704128 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 23434: 26 fields, fnv 6bb46cc2
                { 23434, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 57 }, { "eff0.EffectBasePoints", 50 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 13843: 49 fields, fnv c66a1548
                { 13843, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 54 }, { "eff0.EffectBasePoints", 6 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 55 }, { "eff1.EffectBasePoints", 6 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44457: 49 fields, fnv 87d07253
                { 44457, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 3000 }, { "eff0.EffectBasePoints", 153 }, { "eff0.EffectBonusCoefficient", 1048844435 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 44461 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 44461: 26 fields, fnv f40ea43f
                { 44461, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 306 }, { "eff0.EffectBonusCoefficient", 1057216266 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_1", 13 }, { "eff0.EffectSpellClassMask.Flags", 12582935 }, { "eff0.ImplicitTarget_0", 53 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 86473: 26 fields, fnv d7de8265
                { 86473, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 318 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 76595: 49 fields, fnv ab04106a
                { 76595, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBonusCoefficient", 1077097267 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 22 }, { "eff0.EffectSpellClassMask.Flags", 580542277877760 }, { "eff0.EffectSpellClassMask.Flags2", 1024 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectBasePoints", 280 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
            };
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            SpellEntry const* combustionSpell = sSpellStore.LookupEntry(kCombustionDot);
            if (!combustionSpell)
            {
                return;
            }
            if (k == kGainStep && !atRead)
            {
                const uint32 unit = 100000;
                const uint32 before = p->SpellDamageBonusDone(target, combustionSpell, unit, DOT, 1);
                p->ApplyRatingMod(CR_MASTERY, kMasteryGain, true);
                const uint32 after = p->SpellDamageBonusDone(target, combustionSpell, unit, DOT, 1);
                st.seen[k].reads["flashburnBefore"] = before;
                st.seen[k].reads["flashburnAfter"] = after;
                m_rec.Note("his mastery rating raised by " + std::to_string(kMasteryGain) + ": mastery " +
                           std::to_string(p->GetFloatValue(PLAYER_MASTERY)) + ", Flashburn " + U(before) + " -> " + U(after) + " per " + U(unit));
                return;
            }
            if (k == kCombustionStep && !atRead)
            {
                Aura const* ignite = target->GetAura(kIgniteDot, EFFECT_INDEX_0);
                Aura const* pyroblast = target->GetAura(kPyroblast, EFFECT_INDEX_1);
                Aura const* glyph = target->GetAura(kFrostfireBolt, EFFECT_INDEX_2);
                SpellEntry const* pyroblastSpell = sSpellStore.LookupEntry(kPyroblast);
                if (!ignite || !pyroblast || !glyph || !pyroblastSpell || glyph->GetModifier()->periodictime == 0)
                {
                    m_rec.Note(std::string("a DoT is missing at the cast: Ignite ") + (ignite ? "held" : "absent") + ", Pyroblast " +
                               (pyroblast ? "held" : "absent") + ", Frostfire Bolt " + (glyph ? "held" : "absent"));
                    return;
                }
                const double flashburn = double(p->SpellDamageBonusDone(target, combustionSpell, 100000, DOT, 1)) / 100000.0;
                const double before = double(Kept(st, kGainStep, "flashburnBefore")) / 100000.0;
                const double igniteBase = double(ignite->GetTickAmountBeforeDone());
                const double glyphBase = double(glyph->GetTickAmountBeforeDone());
                const double glyphPeriod = double(glyph->GetModifier()->periodictime);
                const double pyroblastDone = double(p->SpellDamageBonusDone(target, pyroblastSpell, uint32(pyroblast->GetTickAmountBeforeDone()),
                                                                          DOT, pyroblast->GetStackAmount()));
                const double sum = igniteBase * 1000.0 / double(kIgnitePeriod) + pyroblastDone * 1000.0 / double(kShareDbcPeriod) / flashburn +
                                   glyphBase * 1000.0 / glyphPeriod;
                const uint32 done = p->SpellDamageBonusDone(target, combustionSpell, uint32(sum), DOT, 1);
                Seen& seen = st.seen[k];
                seen.reads["ignite"] = int64(igniteBase);
                seen.reads["igniteStored"] = ignite->GetModifier()->m_amount;
                seen.reads["glyph"] = int64(glyphBase);
                seen.reads["glyphPeriod"] = int64(glyphPeriod);
                seen.reads["glyphStored"] = glyph->GetModifier()->m_amount;
                seen.reads["pyroblast"] = int64(pyroblastDone);
                seen.reads["stacks"] = glyph->GetStackAmount();
                seen.reads["moveIgnite"] = int64(igniteBase * (flashburn - before) * 1000.0 / double(kIgnitePeriod));
                seen.reads["moveGlyph"] = int64(glyphBase * (flashburn - before) * 1000.0 / glyphPeriod);
                seen.reads["wantLow"] = target->SpellDamageBonusTaken(p, combustionSpell, done > 0 ? done - 1 : 0, DOT, 1);
                seen.reads["wantHigh"] = target->SpellDamageBonusTaken(p, combustionSpell, done + 1, DOT, 1);
                m_rec.Note("at the cast: Flashburn " + std::to_string(flashburn) + " (" + std::to_string(before) + " at the hits), Ignite " +
                           U(uint32(igniteBase)) + " (stored " + U(uint32(ignite->GetModifier()->m_amount)) + "), Pyroblast " + U(uint32(pyroblastDone)) +
                           ", Frostfire " + U(uint32(glyphBase)) + " over " + U(uint32(glyphPeriod)) + " (stored " + U(uint32(glyph->GetModifier()->m_amount)) +
                           ", " + U(glyph->GetStackAmount()) + " stacks); sum a second " + std::to_string(sum) + ", through Combustion " + U(done));
            }
        }

        std::string Own(St const& st) const override
        {
            std::vector<std::pair<uint32, bool> > ticks;
            Trace::Roles const& roles = m_rec.Roles();
            for (uint32 k = 0; k < st.steps.size(); ++k)
            {
                const char* suffixes[] = { "", "+tick", "-read", "-read+tick" };
                for (const char* suffix : suffixes)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn("step" + U(k + 1) + suffix, SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG, &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.%u=%u/%u/%u/%u", &spell, &amount, &crit, &absorbed, &resisted) == 5 &&
                                spell == kCombustionDot)
                            {
                                ticks.push_back(std::make_pair(amount, crit != 0));
                            }
                        }
                    }
                }
            }

            const int64 low = Kept(st, kCombustionStep, "wantLow");
            const int64 high = Kept(st, kCombustionStep, "wantHigh");
            const int64 before = Kept(st, kGainStep, "flashburnBefore");
            const int64 after = Kept(st, kGainStep, "flashburnAfter");
            const int64 moveIgnite = Kept(st, kCombustionStep, "moveIgnite");
            const int64 moveGlyph = Kept(st, kCombustionStep, "moveGlyph");
            const uint32 first = ticks.empty() ? 0 : ticks[0].first;

            // each part's tick before its mastery, times the Flashburn at its hit, is the tick its aura stores
            const double hitFlashburn = double(before) / 100000.0;
            const int64 igniteBase = Kept(st, kCombustionStep, "ignite");
            const int64 igniteStored = Kept(st, kCombustionStep, "igniteStored");
            const int64 glyphBase = Kept(st, kCombustionStep, "glyph");
            const int64 glyphStored = Kept(st, kCombustionStep, "glyphStored");
            const int64 igniteAtHit = int64(double(igniteBase) * hitFlashburn);
            const int64 glyphAtHit = int64(double(glyphBase) * hitFlashburn);
            const bool baseIsStoredAtHit = igniteBase > 0 && glyphBase > 0 && igniteAtHit - igniteStored <= 1 &&
                                           igniteStored - igniteAtHit <= 1 && glyphAtHit - glyphStored <= 1 &&
                                           glyphStored - glyphAtHit <= 1;

            bool ok = st.steps.size() == 7 && before > 0 && after > before && low >= 0 && high >= low && ticks.size() >= 9 &&
                      baseIsStoredAtHit && moveIgnite >= kMinMove && moveGlyph >= kMinMove && int64(first) >= low &&
                      int64(first) <= high;
            std::string tickText;
            for (size_t t = 0; t < ticks.size(); ++t)
            {
                ok = ok && ticks[t].first == first && !ticks[t].second;
                tickText += (t ? " " : "") + U(ticks[t].first) + (ticks[t].second ? "c" : "");
            }

            const std::string value = "tick=" + (ticks.empty() ? std::string("none") : U(first)) + ",ticks=" + U(ticks.size()) + ",want=" +
                                      std::to_string(low) + ".." + std::to_string(high) + ",flashburn=" + std::to_string(before) + "/" +
                                      std::to_string(after) + ",moves=" + std::to_string(moveIgnite) + "/" + std::to_string(moveGlyph);
            char buf[1024];
            snprintf(buf, sizeof(buf),
                     "83853 ticks [%s]; Ignite before its mastery %lld (stored %lld), Pyroblast at the cast %lld, Frostfire %lld over %lld "
                     "(stored %lld, %lld stacks); at the hits' Flashburn the Ignite part is %lld and the glyph part %lld; "
                     "the tick wanted is %lld..%lld; moving the masteries moves the Ignite part %lld and the glyph part %lld a second",
                     tickText.c_str(), (long long)igniteBase, (long long)igniteStored, (long long)Kept(st, kCombustionStep, "pyroblast"),
                     (long long)glyphBase, (long long)Kept(st, kCombustionStep, "glyphPeriod"), (long long)glyphStored,
                     (long long)Kept(st, kCombustionStep, "stacks"), (long long)igniteAtHit, (long long)glyphAtHit, (long long)low,
                     (long long)high, (long long)moveIgnite, (long long)moveGlyph);
            return value + (ok ? "(OK: " : "(BUG: ") + buf + ")";
        }

    private:
        static constexpr uint32 kPyroblast = 11366;
        static constexpr uint32 kCombustion = 11129;
        static constexpr uint32 kCombustionDot = 83853;
        static constexpr uint32 kIgniteDot = 12654;
        static constexpr uint32 kFrostfireBolt = 44614;
        static constexpr uint32 kGlyphOfFrostfireSlot = 591;
        static constexpr uint32 kMasteryPassive = 86473;
        static constexpr uint32 kPrecision = 13843;
        static constexpr int32 kSpellPower = 1000;
        static constexpr int32 kMasteryRating = 1000;
        static constexpr int32 kMasteryGain = 3000;
        static constexpr int64 kMinMove = 3;
        static constexpr uint32 kIgnitePeriod = 2000;
        static constexpr uint32 kShareDbcPeriod = 3000;
        static constexpr uint32 kBoltEveryMs = 3300;
        static constexpr uint32 kBoltReadMs = 3100;
        static constexpr uint32 kPyroblastEveryMs = 4300;
        static constexpr uint32 kGainWaitMs = 1000;
        static constexpr uint32 kCombustionEveryMs = 1000;
        static constexpr uint32 kLastStepMs = 11500;
        static constexpr uint32 kGainStep = 4;
        static constexpr uint32 kCombustionStep = 5;
        static constexpr int64 kNone = -1;

        /// A read kept by ReadOwn for step k, kNone when the step kept none of that name.
        static int64 Kept(St const& st, uint32 k, char const* name)
        {
            if (k >= st.seen.size())
            {
                return kNone;
            }
            std::map<std::string, int64>::const_iterator i = st.seen[k].reads.find(name);
            return i == st.seen[k].reads.end() ? kNone : i->second;
        }
    };

    /**
     * S1027 `mage-fire-ignite-munching`: two Ignite crits that land in the same server update feed Ignite once, the
     * later share in place of the earlier; two that land a few updates apart both feed it; a pair that lands on a
     * running Ignite rolls over what that Ignite had left and the later share; and Pyroblast! (92315) leaves
     * Clearcasting (12536) on. A Fire mage staged as MageFireProofScenario stages him, but with Burning Soul 3,
     * Improved Fire Blast 2 and Ignite 3 (12846), and Pyroblast! learned. The Mastery passive is not learned, so his
     * mastery is 0 and Flashburn adds nothing to Ignite. Each Fireball crit casts 12654 on the Camel with 40% of the
     * crit's damage, as an integer, over the DoT's two ticks; a crit before the next tick of his Ignite replaces it,
     * carries what it had left and keeps that tick's time, so the new Ignite deals three ticks. Steps:
     *  1. a wait: Precision (13843) and the crit aura at +100 cast on him, then two Fireballs (133) cast at the Camel
     *     by call, triggered, one after the other: both fly the same ten yards and land in one later update;
     *  2. a wait of kGapMs: one Fireball cast the same way;
     *  3. a wait: one more Fireball, which lands kGapMs after the one of step 2, before that one's Ignite ticks;
     *  4. a wait of kGapMs: one Fireball cast the same way;
     *  5. a wait: two more Fireballs cast as in step 1, which land in one update kGapMs after the one of step 4, before
     *     that one's Ignite ticks;
     *  6. Hot Streak (48108) cast at himself, then Precision again and the crit aura at -100, both before
     *     Clearcasting is on him;
     *  7. Clearcasting cast at himself: one charge;
     *  8. Pyroblast! through the handler's calls with Hot Streak and Clearcasting on him; it must not crit.
     * Steps 1, 3 and 5 wait kSettleMs, so each Ignite ticks out inside its step's windows.
     *
     * `munching`: `pair=<tick(D1)>+<tick(D2)>:<the 12654 ticks after them>,spaced=<tick(DA)>+<tick(DB)>:<the 12654
     * ticks after them>,over=<tick(DR)>+<tick(D3)>+<tick(D4)>:<the 12654 ticks after them>,clearcasting=<kept or
     * gone>`, with tick(D) = int(D x 40 / 100) / 2 for a crit of D, the crits in the order their damage logs came. OK
     * when every Fireball crit; the pair's Ignite ticks three times for (2 x tick(D2)) / 3, the later share alone; the
     * spaced crits' Ignite ticks three times for (2 x tick(DB) + 2 x tick(DA)) / 3, both shares; the pair on the
     * running Ignite of step 4 ticks three times for (2 x tick(D4) + 3 x ((2 x tick(DR)) / 3)) / 3, the later share
     * and what the running Ignite had left over three ticks; and Clearcasting is on him when step 8 runs, right after
     * the cast and at the read.
     */
    class MageFireIgniteMunching : public MageFireProofScenario
    {
    public:
        MageFireIgniteMunching() : MageFireProofScenario("mage-fire-ignite-munching", 1027, "munching") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        std::vector<uint32> Spells() const override { return { kPyroblastInstant }; }
        uint32 Watched() const override { return kClearcasting; }

        std::vector<Step> Steps() const override
        {
            return { Wait(kSettleMs), Wait(kGapMs), Wait(kSettleMs), Wait(kGapMs), Wait(kSettleMs),
                     SelfAura(kHotStreak, kAuraEveryMs), SelfAura(kClearcasting, kAuraEveryMs),
                     Cast(kPyroblastInstant, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 92315: 49 fields, fnv 7ef90b4a
                { 92315, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 140 }, { "eff0.EffectBonusCoefficient", 1069925007 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 47 }, { "eff0.EffectRealPointsPerLevel", 1072902963 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 3000 }, { "eff1.EffectBasePoints", 14 }, { "eff1.EffectBonusCoefficient", 1043878380 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 48108: 26 fields, fnv 28b4e817
                { 48108, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 332 }, { "eff0.EffectBasePoints", 92315 }, { "eff0.EffectBonusCoefficient", 1065353216 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 10 }, { "eff0.EffectSpellClassMask.Flags2", 4096 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12536: 26 fields, fnv 1683686a
                { 12536, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 721555055317751 }, { "eff0.EffectSpellClassMask.Flags2", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                return;
            }
            if (k == kPairStep)
            {
                p->CastSpell(p, kPrecision, true);
                int32 points = kCritAuraPoints;
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
            }
            if (k == kPairStep || k == kSpacedStep || k == kSpacedStep + 1 || k == kOverStep || k == kOverStep + 1)
            {
                const uint32 casts = k == kPairStep || k == kOverStep + 1 ? 2 : 1;
                for (uint32 i = 0; i < casts; ++i)
                {
                    p->CastSpell(target, kFireball, true);
                }
                char buf[160];
                snprintf(buf, sizeof(buf), "spell crit %.2f%% (fire); Fireballs cast at the Camel by call: %u",
                         p->GetFloatValue(PLAYER_SPELL_CRIT_PERCENTAGE1 + SPELL_SCHOOL_FIRE), casts);
                m_rec.Note(buf);
            }
            else if (k == kHotStreakStep)
            {
                p->CastSpell(p, kPrecision, true);
                p->RemoveAurasDueToSpell(kCritAura);
                int32 points = NonCritAuraPoints();
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
                m_rec.Note("Precision cast at him again and the crit aura at " + std::to_string(points) +
                           "; 48108 on him: " + HolderOf(p, kHotStreak));
            }
            else if (k == kPyroblastStep)
            {
                st.seen[k].reads["clearcasting"] = p->GetSpellAuraHolder(kClearcasting) ? 1 : 0;
                m_rec.Note("12536 on him right after the cast: " + HolderOf(p, kClearcasting));
            }
        }

        std::string Own(St const& st) const override
        {
            const std::vector<std::pair<uint32, bool> > pair = Hits(kPairStep, kPairStep);
            const std::vector<std::pair<uint32, bool> > spaced = Hits(kSpacedStep, kSpacedStep + 1);
            const std::vector<uint32> pairTicks = IgniteTicks(kPairStep, kPairStep);
            const std::vector<uint32> spacedTicks = IgniteTicks(kSpacedStep, kSpacedStep + 1);

            const bool pairCrit = pair.size() == 2 && pair[0].second && pair[1].second;
            const bool spacedCrit = spaced.size() == 2 && spaced[0].second && spaced[1].second;
            const uint32 first = pair.size() == 2 ? Share(pair[0].first) : 0;
            const uint32 second = pair.size() == 2 ? Share(pair[1].first) : 0;
            const uint32 spacedFirst = spaced.size() == 2 ? Share(spaced[0].first) : 0;
            const uint32 spacedSecond = spaced.size() == 2 ? Share(spaced[1].first) : 0;
            const uint32 laterAlone = 2 * second / kRefreshedTicks;
            const uint32 pairBoth = (2 * second + 2 * first) / kRefreshedTicks;
            const uint32 spacedBoth = (2 * spacedSecond + 2 * spacedFirst) / kRefreshedTicks;
            const bool pairOk = pairCrit && AllAre(pairTicks, laterAlone);
            const bool spacedOk = spacedCrit && AllAre(spacedTicks, spacedBoth);

            // the pair on a running Ignite: the later share and what the running one had left, spread over three ticks
            const std::vector<std::pair<uint32, bool> > over = Hits(kOverStep, kOverStep + 1);
            const std::vector<uint32> overTicks = IgniteTicks(kOverStep, kOverStep + 1);
            const bool overCrit = over.size() == 3 && over[0].second && over[1].second && over[2].second;
            const uint32 running = over.size() == 3 ? Share(over[0].first) : 0;
            const uint32 overFirst = over.size() == 3 ? Share(over[1].first) : 0;
            const uint32 overSecond = over.size() == 3 ? Share(over[2].first) : 0;
            const uint32 runningLeft = kRefreshedTicks * (kShareTicks * running / kRefreshedTicks);
            const uint32 overWanted = (kShareTicks * overSecond + runningLeft) / kRefreshedTicks;
            const bool overOk = overCrit && AllAre(overTicks, overWanted);

            Seen const& cast = st.seen[kPyroblastStep];
            const bool heldBefore = !cast.watchedBefore.empty() && cast.watchedBefore != "none";
            const bool keptAtCast = Kept(st, kPyroblastStep, "clearcasting") == 1;
            const bool keptAtRead = cast.read && !cast.holder.empty() && cast.holder != "none";
            const bool clearcastingOk = heldBefore && keptAtCast && keptAtRead && cast.goSeen;
            const std::string clearcasting = !cast.ran ? "unread" : keptAtCast && keptAtRead ? "kept" : "gone";

            const std::string value = "pair=" + U(first) + "+" + U(second) + ":" + TickText(pairTicks) + ",spaced=" +
                                      U(spacedFirst) + "+" + U(spacedSecond) + ":" + TickText(spacedTicks) + ",over=" +
                                      U(running) + "+" + U(overFirst) + "+" + U(overSecond) + ":" +
                                      TickText(overTicks) + ",clearcasting=" + clearcasting;
            char buf[896];
            snprintf(buf, sizeof(buf),
                     "the pair: Fireball %s, its Ignite's ticks [%s], wanted three of (2 x %u) / 3 = %u, the later "
                     "share alone (both shares would give %u); the spaced crits: Fireball %s, ticks [%s], wanted three "
                     "of (2 x %u + 2 x %u) / 3 = %u; the pair on a running Ignite: Fireball %s, ticks [%s], wanted "
                     "three of (2 x %u + %u) / 3 = %u, the later share and the running Ignite's %u left; 12536 on him "
                     "when the Pyroblast! step ran: %s, right after its cast: %s, at its read: %s; the Pyroblast! "
                     "spell go %s",
                     HitsText(pair).c_str(), TickText(pairTicks).c_str(), second, laterAlone, pairBoth,
                     HitsText(spaced).c_str(), TickText(spacedTicks).c_str(), spacedSecond, spacedFirst, spacedBoth,
                     HitsText(over).c_str(), TickText(overTicks).c_str(), overSecond, runningLeft, overWanted,
                     runningLeft, cast.watchedBefore.empty() ? "unread" : cast.watchedBefore.c_str(),
                     keptAtCast ? "held" : "gone", cast.holder.empty() ? "unread" : cast.holder.c_str(),
                     cast.goSeen ? "seen" : "not seen");
            const bool ok = pairOk && spacedOk && overOk && clearcastingOk;
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kPyroblastInstant = 92315;
        static constexpr uint32 kHotStreak = 48108;
        static constexpr uint32 kClearcasting = 12536;
        static constexpr uint32 kIgnitePercent = 40;
        static constexpr uint32 kShareTicks = 2;
        static constexpr uint32 kRefreshedTicks = 3;
        static constexpr uint32 kSettleMs = 8000;
        static constexpr uint32 kGapMs = 350;
        static constexpr uint32 kAuraEveryMs = 500;
        static constexpr uint32 kPairStep = 0;
        static constexpr uint32 kSpacedStep = 1;
        static constexpr uint32 kOverStep = 3;
        static constexpr uint32 kHotStreakStep = 5;
        static constexpr uint32 kPyroblastStep = 7;

        /// Ignite's share a tick of a crit of `amount`: 40% of it as an integer, over the DoT's two ticks.
        static uint32 Share(uint32 amount)
        {
            return uint32(uint64(amount) * kIgnitePercent / 100) / kShareTicks;
        }

        /// Whether `ticks` holds three ticks, each `want`.
        static bool AllAre(std::vector<uint32> const& ticks, uint32 want)
        {
            if (ticks.size() != kRefreshedTicks)
            {
                return false;
            }
            for (size_t i = 0; i < ticks.size(); ++i)
            {
                if (ticks[i] != want)
                {
                    return false;
                }
            }
            return true;
        }

        /// The ticks as "a/b/c", or "none".
        static std::string TickText(std::vector<uint32> const& values)
        {
            std::string out;
            for (size_t i = 0; i < values.size(); ++i)
            {
                out += (i ? "/" : "") + U(values[i]);
            }
            return out.empty() ? std::string("none") : out;
        }

        /// The hits as "amount crit, amount", or "none".
        static std::string HitsText(std::vector<std::pair<uint32, bool> > const& hits)
        {
            std::string out;
            for (size_t i = 0; i < hits.size(); ++i)
            {
                out += (i ? ", " : "") + U(hits[i].first) + (hits[i].second ? " crit" : "");
            }
            return out.empty() ? std::string("none") : out;
        }

        /// His Fireball hits on the Camel in the windows of steps `from` to `to` (0-based), in order: the amount and
        /// whether it crit.
        std::vector<std::pair<uint32, bool> > Hits(uint32 from, uint32 to) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<std::pair<uint32, bool> > hits;
            for (uint32 k = from; k <= to; ++k)
            {
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
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG,
                                                                                 &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        uint32 spell = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                        if (fields.size() == 1 &&
                            sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &spell, &amount, &crit,
                                   &absorbed, &resisted) == 5 &&
                            spell == kFireball)
                        {
                            hits.push_back(std::make_pair(amount, crit != 0));
                        }
                    }
                }
            }
            return hits;
        }

        /// The 12654 ticks the Camel took from him in the windows of steps `from` to `to` (0-based), in order.
        std::vector<uint32> IgniteTicks(uint32 from, uint32 to) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            std::vector<uint32> ticks;
            for (uint32 k = from; k <= to; ++k)
            {
                const std::vector<std::string> windows = StepWindows(k);
                for (size_t w = 0; w < windows.size(); ++w)
                {
                    std::vector<Recorder::Seen const*> seen = m_rec.SeenIn(windows[w], SMSG_PERIODICAURALOG);
                    for (size_t i = 0; i < seen.size(); ++i)
                    {
                        if (seen[i]->payload.empty())
                        {
                            continue;
                        }
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_PERIODICAURALOG,
                                                                                 &seen[i]->payload[0],
                                                                                 seen[i]->payload.size(), roles);
                        for (size_t f = 0; f < fields.size(); ++f)
                        {
                            uint32 amount = 0;
                            if (sscanf(fields[f].c_str(), "tick self.tick.12654=%u/", &amount) == 1)
                            {
                                ticks.push_back(amount);
                            }
                        }
                    }
                }
            }
            return ticks;
        }
    };

    /**
     * S1028 `mage-fire-clearcasting`: Clearcasting (12536) is used up only by a spell it makes cheaper, a mage spell of
     * its class mask with a mana cost. A Fire mage staged as 1027 stages him, without Pyroblast!. Before each act but
     * the first and the Blizzard's, Clearcasting is cast at himself, so he holds its one charge. Steps, kActMs apart
     * unless named, every cast by call and triggered unless named:
     *  1. a wait of kAuraEveryMs: Precision (13843) and the crit aura at -100 cast at him, before any Clearcasting;
     *  2. a wait: Blood Fury (33702, an orc mage's racial, a spell with no class options) cast at himself;
     *  3. a wait: the free Flamestrike (88148, no mana cost) cast at the Camel's point, as Blast Wave casts it;
     *  4. a wait of kChannelMs: Blizzard (10) cast at the Camel's point, then Clearcasting: a Blizzard cast by call
     *     runs no channel, and its damage (42208, no mana cost) comes every second from the aura it puts on him;
     *  5. a wait: Clearcasting read, then cast again;
     *  6. Fireball (133) through the handler's calls, which costs mana; it must not crit.
     * Steps 2, 3 and 5 read Clearcasting on him right after the act (step 5 before it casts Clearcasting again), step 6
     * at its read.
     *
     * `clearcasting`: `racial=<kept or gone>,flamestrike=<kept or gone>/<its hits on the Camel in step 3's
     * windows>,blizzard=<kept or gone>/<42208's hits in steps 4 and 5>,fireball=<kept or gone>`. OK when Clearcasting
     * is kept through Blood Fury, the free Flamestrike and Blizzard's damage, each of the two hit the Camel, and
     * the Fireball, whose spell go came, used it up.
     */
    class MageFireClearcasting : public MageFireProofScenario
    {
    public:
        MageFireClearcasting() : MageFireProofScenario("mage-fire-clearcasting", 1028, "clearcasting") {}

    protected:
        std::vector<std::pair<uint32, uint32> > Talents() const override
        {
            return { { 10531, 2 }, { 10523, 1 }, { 10529, 2 } };
        }
        uint32 Watched() const override { return kClearcasting; }

        std::vector<Step> Steps() const override
        {
            return { Wait(kAuraEveryMs), Wait(kActMs), Wait(kActMs), Wait(kChannelMs), Wait(kActMs),
                     Cast(kFireball, false) };
        }

        std::vector<SpellPrint> Prints() const override
        {
            return ProofPrints({
                // 133: 26 fields, fnv 81dba6e7
                { 133, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 13 }, { "eff0.EffectBonusCoefficient", 1067332927 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 9 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 84254: 26 fields, fnv 68c83b92
                { 84254, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", 70 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 9 }, { "eff0.EffectSpellClassMask.Flags", 26389977761973 }, { "eff0.EffectSpellClassMask.Flags2", 192 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 11080: 49 fields, fnv ce553bfe
                { 11080, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 107 }, { "eff0.EffectBasePoints", 8 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 7 }, { "eff0.EffectSpellClassMask.Flags", 2 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 107 }, { "eff1.EffectBasePoints", 10 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 5 }, { "eff1.EffectSpellClassMask.Flags", 2 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12846: 26 fields, fnv 378f106d
                { 12846, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 40 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 12654: 49 fields, fnv ac3e5e56
                { 12654, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 3 }, { "eff0.EffectAuraPeriod", 2000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 6 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 4 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.ImplicitTarget_0", 6 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 12536: 26 fields, fnv 1683686a
                { 12536, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 108 }, { "eff0.EffectBasePoints", -1000 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectMiscValue_0", 14 }, { "eff0.EffectSpellClassMask.Flags", 721555055317751 }, { "eff0.EffectSpellClassMask.Flags2", 2048 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
                // 33702: 49 fields, fnv 7b0ec03a
                { 33702, { { "eff0.present", 1 }, { "eff0.Effect", 6 }, { "eff0.EffectAura", 135 }, { "eff0.EffectBasePoints", 5 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.ImplicitTarget_0", 1 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 13 }, { "eff1.EffectBasePoints", 5 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectMiscValue_0", 126 }, { "eff1.EffectRealPointsPerLevel", 1073741824 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 88148: 49 fields, fnv d69ff162
                { 88148, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 51 }, { "eff0.EffectBonusCoefficient", 1041596678 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectDieSides", 17 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1058642330 }, { "eff0.ImplicitTarget_0", 16 }, { "eff1.present", 1 }, { "eff1.Effect", 27 }, { "eff1.EffectAura", 3 }, { "eff1.EffectAuraPeriod", 2000 }, { "eff1.EffectBasePoints", 12 }, { "eff1.EffectBonusCoefficient", 1031396131 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectRadiusIndex_0", 14 }, { "eff1.ImplicitTarget_0", 28 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 10: 49 fields, fnv 5c992065
                { 10, { { "eff0.present", 1 }, { "eff0.Effect", 27 }, { "eff0.EffectAura", 4 }, { "eff0.EffectBasePoints", 25 }, { "eff0.EffectBonusCoefficient", 1039382086 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 28 }, { "eff1.present", 1 }, { "eff1.Effect", 6 }, { "eff1.EffectAura", 23 }, { "eff1.EffectAuraPeriod", 1000 }, { "eff1.EffectChainAmplitude", 1065353216 }, { "eff1.EffectTriggerSpell", 42208 }, { "eff1.ImplicitTarget_0", 1 }, { "eff1.EffectIndex", 1 }, { "eff2.present", 0 } } },
                // 42208: 26 fields, fnv 6c68b897
                { 42208, { { "eff0.present", 1 }, { "eff0.Effect", 2 }, { "eff0.EffectBasePoints", 36 }, { "eff0.EffectBonusCoefficient", 1042670420 }, { "eff0.EffectChainAmplitude", 1065353216 }, { "eff0.EffectRadiusIndex_0", 14 }, { "eff0.EffectRadiusIndex_1", 14 }, { "eff0.EffectRealPointsPerLevel", 1036831949 }, { "eff0.ImplicitTarget_0", 76 }, { "eff0.ImplicitTarget_1", 16 }, { "eff1.present", 0 }, { "eff2.present", 0 } } },
            });
        }

        void ReadOwn(St& st, uint32 k, bool atRead, Player* p, Creature* target) override
        {
            if (atRead)
            {
                return;
            }
            Seen& seen = st.seen[k];
            if (k == kPrecisionStep)
            {
                p->CastSpell(p, kPrecision, true);
                int32 points = NonCritAuraPoints();
                p->CastCustomSpell(p, kCritAura, &points, NULL, NULL, true);
                m_rec.Note("Precision and the crit aura at " + std::to_string(points) + " cast at him");
            }
            else if (k == kRacialStep)
            {
                PutClearcasting(p);
                p->CastSpell(p, kBloodFury, true);
                seen.reads["clearcasting"] = Holds(p);
                m_rec.Note("Blood Fury cast at him; 12536 on him right after: " + HolderOf(p, kClearcasting));
            }
            else if (k == kFlamestrikeStep)
            {
                PutClearcasting(p);
                p->CastSpell(target->Where().X(), target->Where().Y(), target->Where().Z(), kFreeFlamestrike, true);
                seen.reads["clearcasting"] = Holds(p);
                m_rec.Note("the free Flamestrike cast at the Camel's point; 12536 on him right after: " +
                           HolderOf(p, kClearcasting));
            }
            else if (k == kBlizzardStep)
            {
                p->CastSpell(target->Where().X(), target->Where().Y(), target->Where().Z(), kBlizzard, true);
                const bool channel = p->GetCurrentSpell(CURRENT_CHANNELED_SPELL) != NULL;
                PutClearcasting(p);
                m_rec.Note(std::string("Blizzard cast at the Camel's point, his channel ") + (channel ? "on" : "off") +
                           "; then 12536 cast at him: " + HolderOf(p, kClearcasting));
            }
            else if (k == kBlizzardReadStep)
            {
                seen.reads["clearcasting"] = Holds(p);
                m_rec.Note("12536 on him after Blizzard's damage: " + HolderOf(p, kClearcasting));
                PutClearcasting(p);
            }
        }

        std::string Own(St const& st) const override
        {
            const int64 racial = Kept(st, kRacialStep, "clearcasting");
            const int64 flamestrike = Kept(st, kFlamestrikeStep, "clearcasting");
            const int64 blizzard = Kept(st, kBlizzardReadStep, "clearcasting");
            const uint32 flamestrikeHits = HitsOf(kFreeFlamestrike, kFlamestrikeStep, kFlamestrikeStep);
            const uint32 blizzardHits = HitsOf(kBlizzardDamage, kBlizzardStep, kBlizzardReadStep);
            Seen const& cast = st.seen[kFireballStep];
            const bool fireballRead = cast.read && !cast.holder.empty();
            const bool fireballKept = fireballRead && cast.holder != "none";

            const std::string value = "racial=" + KeptText(racial) + ",flamestrike=" + KeptText(flamestrike) + "/" +
                                      U(flamestrikeHits) + ",blizzard=" + KeptText(blizzard) + "/" + U(blizzardHits) +
                                      ",fireball=" + (!fireballRead ? "unread" : fireballKept ? "kept" : "gone");
            char buf[320];
            snprintf(buf, sizeof(buf),
                     "12536 on him right after Blood Fury: %s, the free Flamestrike: %s (its hits %u), Blizzard's "
                     "damage: %s (42208 hits %u); at the Fireball's read: %s, its spell go %s",
                     KeptText(racial).c_str(), KeptText(flamestrike).c_str(), flamestrikeHits,
                     KeptText(blizzard).c_str(), blizzardHits, cast.holder.empty() ? "unread" : cast.holder.c_str(),
                     cast.goSeen ? "seen" : "not seen");
            const bool ok = racial == 1 && flamestrike == 1 && blizzard == 1 && flamestrikeHits >= 1 &&
                            blizzardHits >= 1 && fireballRead && !fireballKept && cast.goSeen;
            return ok ? value + "(OK: " + buf + ")" : "BUG(" + value + ": " + buf + ")";
        }

    private:
        static constexpr uint32 kFireball = 133;
        static constexpr uint32 kClearcasting = 12536;
        static constexpr uint32 kBloodFury = 33702;
        static constexpr uint32 kFreeFlamestrike = 88148;
        static constexpr uint32 kBlizzard = 10;
        static constexpr uint32 kBlizzardDamage = 42208;
        static constexpr uint32 kAuraEveryMs = 500;
        static constexpr uint32 kActMs = 1000;
        static constexpr uint32 kChannelMs = 2600;
        static constexpr uint32 kPrecisionStep = 0;
        static constexpr uint32 kRacialStep = 1;
        static constexpr uint32 kFlamestrikeStep = 2;
        static constexpr uint32 kBlizzardStep = 3;
        static constexpr uint32 kBlizzardReadStep = 4;
        static constexpr uint32 kFireballStep = 5;

        /// Clearcasting cast at him, triggered: one charge.
        static void PutClearcasting(Player* p)
        {
            p->CastSpell(p, kClearcasting, true);
        }

        /// 1 when he holds Clearcasting, 0 when not.
        static int64 Holds(Player* p)
        {
            return p->GetSpellAuraHolder(kClearcasting) ? 1 : 0;
        }

        /// "kept" for 1, "gone" for 0, "unread" otherwise.
        static std::string KeptText(int64 held)
        {
            return held == 1 ? "kept" : held == 0 ? "gone" : "unread";
        }

        /// The damage logs of `spell` on the Camel from him in the windows of steps `from` to `to` (0-based).
        uint32 HitsOf(uint32 spell, uint32 from, uint32 to) const
        {
            Trace::Roles const& roles = m_rec.Roles();
            uint32 hits = 0;
            for (uint32 k = from; k <= to; ++k)
            {
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
                        std::vector<std::string> fields = Trace::CombatLogFields(SMSG_SPELLNONMELEEDAMAGELOG,
                                                                                 &logs[i]->payload[0],
                                                                                 logs[i]->payload.size(), roles);
                        uint32 logged = 0, amount = 0, crit = 0, absorbed = 0, resisted = 0;
                        if (fields.size() == 1 &&
                            sscanf(fields[0].c_str(), "hit self.hit.%u=%u/%u/%u/%u", &logged, &amount, &crit,
                                   &absorbed, &resisted) == 5 &&
                            logged == spell)
                        {
                            ++hits;
                        }
                    }
                }
            }
            return hits;
        }
    };

    void RegisterMageFireScenarios(Runner& r)
    {
        r.Register(new MageFireFireballCrit());
        r.Register(new MageFireHotStreak());
        r.Register(new MageFireMasterOfElements());
        r.Register(new MageFireBlazingSpeed());
        r.Register(new MageFireCombustion());
        r.Register(new MageFireLivingBomb());
        r.Register(new MageFirePyroblastHaste());
        r.Register(new MageFirePyroblastRefresh());
        r.Register(new MageFireIgniteMastery());
        r.Register(new MageFireCriticalMass());
        r.Register(new MageFireLivingBombLimit());
        r.Register(new MageFireIgnite());
        r.Register(new MageFireImpact());
        r.Register(new MageFireCauterize());
        r.Register(new MageFireLivingBombRules());
        r.Register(new MageOffensiveStateDamageless());
        r.Register(new MageFireFlameOrb());
        r.Register(new MageFireFlameOrbSkips());
        r.Register(new MageFireFlameOrbDescends());
        r.Register(new MageFireFlameOrbDownhill());
        r.Register(new MageFireFlameOrbClimbs());
        r.Register(new MageFirePyromaniac());
        r.Register(new MageFireGlyphOfFrostfire());
        r.Register(new MageFireFrostfireSnareImmune());
        r.Register(new MageFireDamagePassives());
        r.Register(new MageFireCastPassives());
        r.Register(new MageFireGlyphs());
        r.Register(new MageFireMoltenFuryTick());
        r.Register(new MageFireCombustionMoltenFury());
        r.Register(new MageFireMoltenArmor());
        r.Register(new MageFireMoltenArmorProcs());
        r.Register(new MageFirePyroblastPath());
        r.Register(new MageFireBlazingSpeedDispel());
        r.Register(new MageFireMageWardBreak());
        r.Register(new MageFireImprovedFlamestrike());
        r.Register(new MageFireImprovedFlamestrikeRankOne());
        r.Register(new MageFireInvisibilityFade());
        r.Register(new MageThreatReduction());
        r.Register(new MageFireShareBeforeTaken());
        r.Register(new MageFireCombustionAtTheCast());
        r.Register(new MageFireCombustionMasteryAtTheCast());
        r.Register(new MageFireFlameOrbOwnerHit());
        r.Register(new MageFireFlameOrbOwnerHit("mage-fire-flame-orb-owner-hit-uncapped", 947, false));
        r.Register(new MagePolymorphDamage());
        r.Register(new MageThresholdPreventedDamage());
        r.Register(new MageFireIgniteMunching());
        r.Register(new MageFireClearcasting());
    }
}
