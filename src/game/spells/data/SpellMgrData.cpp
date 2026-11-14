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

/**
 * @file SpellMgrData.cpp
 * @brief SpellMgr's spell target-position, bonus, elixir, threat and script-target loaders.
 */

#include <cstdio>
#include <set>
#include <string>
#include "spells/data/SpellMgr.h"
#include "spells/data/SpellAuraDefines.h"
#include "ObjectMgr.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "ProgressBar.h"
#include "DBCStores.h"
#include "persistence/world/spells/SpellDataRepository.h"
#include "persistence/world/spells/SpellScriptTargetRepository.h"

template <typename EntryType, typename WorkerType, typename StorageType>
struct SpellRankHelper
{
    SpellRankHelper(SpellMgr& _mgr, StorageType& _storage): mgr(_mgr), worker(_storage), customRank(0) {}
    void RecordRank(EntryType& entry, uint32 spell_id)
    {
        const SpellEntry* spell = sSpellStore.LookupEntry(spell_id);
        if (!spell)
        {
            sLog.outErrorDb("Spell %u listed in `%s` does not exist", spell_id, worker.TableName());
            return;
        }

        uint32 first_id = mgr.GetFirstSpellInChain(spell_id);

        // most spell ranks expected same data
        if (first_id)
        {
            firstRankSpells.insert(first_id);

            if (first_id != spell_id)
            {
                if (!worker.IsValidCustomRank(entry, spell_id, first_id))
                {
                    return;
                }
                // for later check that first rank also added
                else
                {
                    firstRankSpellsWithCustomRanks.insert(first_id);
                    ++customRank;
                }
            }
        }

        worker.AddEntry(entry, spell);
    }
    void FillHigherRanks()
    {
        // check that first rank added for custom ranks
        for (std::set<uint32>::const_iterator itr = firstRankSpellsWithCustomRanks.begin(); itr != firstRankSpellsWithCustomRanks.end(); ++itr)
            if (!worker.HasEntry(*itr))
            {
                sLog.outErrorDb("Spell %u must be listed in `%s` as first rank for listed custom ranks of spell but not found!", *itr, worker.TableName());
            }

        // fill absent non first ranks data base at first rank data
        for (std::set<uint32>::const_iterator itr = firstRankSpells.begin(); itr != firstRankSpells.end(); ++itr)
        {
            if (worker.SetStateToEntry(*itr))
            {
                mgr.doForHighRanks(*itr, worker);
            }
        }
    }
    std::set<uint32> firstRankSpells;
    std::set<uint32> firstRankSpellsWithCustomRanks;

    SpellMgr& mgr;
    WorkerType worker;
    uint32 customRank;
};

/**
 * @brief Loads spell target destination coordinates from the database.
 */
void SpellMgr::LoadSpellTargetPositions()
{
    mSpellTargetPositions.clear();                          // need for reload case

    uint32 count = 0;

    std::vector<SpellTargetPositionRow> rows = SpellDataRepository::QueryTargetPositions();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell target destination coordinates", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (SpellTargetPositionRow const& row : rows)
    {
        bar.step();

        uint32 Spell_ID = row.id;

        SpellTargetPosition st;

        st.target_mapId       = row.targetMap;
        st.target_X           = row.targetPositionX;
        st.target_Y           = row.targetPositionY;
        st.target_Z           = row.targetPositionZ;
        st.target_Orientation = row.targetOrientation;

        MapEntry const* mapEntry = sMapStore.LookupEntry(st.target_mapId);
        if (!mapEntry)
        {
            sLog.outErrorDb("Spell (ID:%u) target map (ID: %u) does not exist in `Map.dbc`.", Spell_ID, st.target_mapId);
            continue;
        }

        if (st.target_X == 0 && st.target_Y == 0 && st.target_Z == 0)
        {
            sLog.outErrorDb("Spell (ID:%u) target coordinates not provided.", Spell_ID);
            continue;
        }

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(Spell_ID);
        if (!spellInfo)
        {
            sLog.outErrorDb("Spell (ID:%u) listed in `spell_target_position` does not exist.", Spell_ID);
            continue;
        }

        bool found = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(SpellEffectIndex(i));
            if (!spellEffect)
            {
                continue;
            }
            if (spellEffect->ImplicitTarget_0==TARGET_TABLE_X_Y_Z_COORDINATES || spellEffect->ImplicitTarget_1==TARGET_TABLE_X_Y_Z_COORDINATES)
            {
                found = true;
                break;
            }
        }
        if (!found)
        {
            sLog.outErrorDb("Spell (Id: %u) listed in `spell_target_position` does not have target TARGET_TABLE_X_Y_Z_COORDINATES (17).", Spell_ID);
            continue;
        }

        mSpellTargetPositions[Spell_ID] = st;
        ++count;
    }

    sLog.outString(">> Loaded %u spell target destination coordinates", count);
    sLog.outString();
}

struct DoSpellBonuses
{
    DoSpellBonuses(SpellBonusMap& _spellBonusMap, SpellBonusEntry const& _spellBonus) : spellBonusMap(_spellBonusMap), spellBonus(_spellBonus) {}
    void operator()(uint32 spell_id) { spellBonusMap[spell_id] = spellBonus; }

    SpellBonusMap& spellBonusMap;
    SpellBonusEntry const& spellBonus;
};

/**
 * A row part that a nonzero value of the client's stands beside: the client's value applies and the part is not
 * read. For a nonzero `part` beside a nonzero `reference`, appends "`<name>` <part>, <dbc> holds <reference>" to
 * `found`, after a "; " when `found` already holds one.
 */
static void RowPartNotNeeded(float part, float reference, char const* name, char const* dbc, std::string& found)
{
    if (part == 0.0f || reference == 0.0f)
    {
        return;
    }

    char text[160];
    snprintf(text, sizeof(text), "`%s` %f, %s holds %f", name, part, dbc, reference);
    if (!found.empty())
    {
        found += "; ";
    }
    found += text;
}

/**
 * The spells whose Spell.dbc attack-power coefficient is above 0.
 */
static uint32 CountAttackPowerCoefficients()
{
    uint32 count = 0;
    for (uint32 id = 0; id < sSpellStore.GetNumRows(); ++id)
    {
        SpellEntry const* spell = sSpellStore.LookupEntry(id);
        if (spell && spell->Coefficient > 0.0f)
        {
            ++count;
        }
    }
    return count;
}

/**
 * @brief Loads spell bonus coefficients from the database: the parts that apply where the client's files hold no
 * coefficient, SpellEffect.dbc's for spell power and Spell.dbc's for attack power. Ends with the count of the spells
 * whose Spell.dbc attack-power coefficient is above 0.
 */
void SpellMgr::LoadSpellBonuses()
{
    mSpellBonusMap.clear();                             // need for reload case
    uint32 count = 0;
    std::vector<SpellBonusRow> rows = SpellDataRepository::QueryBonuses();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> Loaded %u spell bonus data", count);
        sLog.outString(">> Spell.dbc holds an attack-power coefficient for %u spells", CountAttackPowerCoefficients());
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());
    for (SpellBonusRow const& row : rows)
    {
        bar.step();
        uint32 entry = row.entry;

        SpellEntry const* spell = sSpellStore.LookupEntry(entry);
        if (!spell)
        {
            sLog.outErrorDb("Spell %u listed in `spell_bonus_data` does not exist", entry);
            continue;
        }

        uint32 first_id = GetFirstSpellInChain(entry);

        if (first_id != entry)
        {
            sLog.outErrorDb("Spell %u listed in `spell_bonus_data` is not first rank (%u) in chain", entry, first_id);
            // prevent loading since it won't have an effect anyway
            continue;
        }

        SpellBonusEntry sbe;

        sbe.direct_damage = row.directBonus;
        sbe.dot_damage    = row.dotBonus;
        sbe.ap_bonus      = row.apBonus;
        sbe.ap_dot_bonus   = row.apDotBonus;

        // The client's coefficients for the row's two spell power parts, as SpellEffect.dbc holds them.
        SpellEffectEntry const* effects[MAX_EFFECT_INDEX];
        for (uint32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            effects[i] = spell->GetSpellEffect(SpellEffectIndex(i));
        }
        float clientDirect = 0.0f;
        float clientDot = 0.0f;
        for (SpellBonusKind kind : { SPELL_BONUS_KIND_DAMAGE, SPELL_BONUS_KIND_HEAL, SPELL_BONUS_KIND_ABSORB })
        {
            if (clientDirect == 0.0f)
            {
                clientDirect = EffectBonusCoefficient(effects, NULL, 0, kind, SPELL_DIRECT_DAMAGE);
            }
            if (clientDot == 0.0f)
            {
                clientDot = EffectBonusCoefficient(effects, NULL, 0, kind, DOT);
            }
        }

        // The client's attack-power coefficient for the row's two attack power parts: Spell.dbc's, where the spell's
        // damage of that type takes it.
        float const clientAp = AttackPowerCoefficient(spell, NULL, SPELL_DIRECT_DAMAGE, true);
        float const clientApDot = AttackPowerCoefficient(spell, NULL, DOT, true);

        std::string notNeeded;
        RowPartNotNeeded(sbe.direct_damage, clientDirect, "direct_bonus", "SpellEffect.dbc", notNeeded);
        RowPartNotNeeded(sbe.dot_damage, clientDot, "dot_bonus", "SpellEffect.dbc", notNeeded);
        RowPartNotNeeded(sbe.ap_bonus, clientAp, "ap_bonus", "Spell.dbc", notNeeded);
        RowPartNotNeeded(sbe.ap_dot_bonus, clientApDot, "ap_dot_bonus", "Spell.dbc", notNeeded);
        if (!notNeeded.empty())
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u is not needed: %s", entry, notNeeded.c_str());
        }
        else if (!sbe.direct_damage && !sbe.dot_damage && !sbe.ap_bonus && !sbe.ap_dot_bonus)
        {
            sLog.outErrorDb("`spell_bonus_data` entry for spell %u holds only zeros: SpellEffect.dbc's coefficients apply", entry);
        }

        mSpellBonusMap[entry] = sbe;

        // also add to high ranks
        DoSpellBonuses worker(mSpellBonusMap, sbe);
        doForHighRanks(entry, worker);

        ++count;
    }

    sLog.outString(">> Loaded %u extra spell bonus data",  count);
    sLog.outString(">> Spell.dbc holds an attack-power coefficient for %u spells", CountAttackPowerCoefficients());
    sLog.outString();
}

/**
 * @brief Loads elixir classification data from the database.
 */
void SpellMgr::LoadSpellElixirs()
{
    mSpellElixirs.clear();                                  // need for reload case

    uint32 count = 0;

    std::vector<SpellElixirRow> rows = SpellDataRepository::QueryElixirs();
    if (rows.empty())
    {
        BarGoLink bar(1);

        bar.step();

        sLog.outString(">> Loaded %u spell elixir definitions", count);
        sLog.outString();
        return;
    }

    BarGoLink bar(rows.size());

    for (SpellElixirRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;
        uint8 mask = row.mask;

        SpellEntry const* spellInfo = sSpellStore.LookupEntry(entry);

        if (!spellInfo)
        {
            sLog.outErrorDb("Spell %u listed in `spell_elixir` does not exist", entry);
            continue;
        }

        mSpellElixirs[entry] = mask;

        ++count;
    }

    sLog.outString(">> Loaded %u spell elixir definitions", count);
    sLog.outString();
}

struct DoSpellThreat
{
    DoSpellThreat(SpellThreatMap& _threatMap) : threatMap(_threatMap), count(0) {}
    void operator()(uint32 spell_id)
    {
        SpellThreatEntry const& ste = state->second;
        // add ranks only for not filled data (spells adding flat threat are usually different for ranks)
        SpellThreatMap::const_iterator spellItr = threatMap.find(spell_id);
        if (spellItr == threatMap.end())
        {
            threatMap[spell_id] = ste;
        }

        // just assert that entry is not redundant
        else
        {
            SpellThreatEntry const& r_ste = spellItr->second;
            if (ste.threat == r_ste.threat && ste.multiplier == r_ste.multiplier && ste.ap_bonus == r_ste.ap_bonus)
            {
                sLog.outErrorDb("Spell %u listed in `spell_threat` as custom rank has same data as Rank 1, so redundant", spell_id);
            }
        }
    }
    const char* TableName() { return "spell_threat"; }
    bool IsValidCustomRank(SpellThreatEntry const& ste, uint32 entry, uint32 first_id)
    {
        if (!ste.threat)
        {
            sLog.outErrorDb("Spell %u listed in `spell_threat` is not first rank (%u) in chain and has no threat", entry, first_id);
            // prevent loading unexpected data
            return false;
        }
        return true;
    }
    void AddEntry(SpellThreatEntry const& ste, SpellEntry const* spell)
    {
        threatMap[spell->ID] = ste;

        // flat threat bonus and attack power bonus currently only work properly when all
        // effects have same targets, otherwise, we'd need to seperate it by effect index
        if (ste.threat || ste.ap_bonus != 0.f)
        {
            SpellEffectEntry const* spellEffect0 = spell->GetSpellEffect(EFFECT_INDEX_0);
            SpellEffectEntry const* spellEffect1 = spell->GetSpellEffect(EFFECT_INDEX_1);
            SpellEffectEntry const* spellEffect2 = spell->GetSpellEffect(EFFECT_INDEX_2);
            if ((spellEffect1 && spellEffect1->ImplicitTarget_0 && (!spellEffect0 || spellEffect1->ImplicitTarget_0 != spellEffect0->ImplicitTarget_0)) ||
                (spellEffect2 && spellEffect2->ImplicitTarget_0 && (!spellEffect0 || spellEffect2->ImplicitTarget_0 != spellEffect0->ImplicitTarget_0)))
                sLog.outErrorDb("Spell %u listed in `spell_threat` has effects with different targets, threat may be assigned incorrectly", spell->ID);
        }
        ++count;
    }
    bool HasEntry(uint32 spellId) { return threatMap.count(spellId) > 0; }
    bool SetStateToEntry(uint32 spellId) { return (state = threatMap.find(spellId)) != threatMap.end(); }

    SpellThreatMap& threatMap;
    SpellThreatMap::const_iterator state;
    uint32 count;
};

/**
 * @brief Loads custom spell threat definitions from the database.
 */
void SpellMgr::LoadSpellThreats()
{
    mSpellThreatMap.clear();                                // need for reload case

    std::vector<SpellThreatRow> rows = SpellDataRepository::QueryThreats();
    if (rows.empty())
    {
        BarGoLink bar(1);
        bar.step();
        sLog.outString(">> No spell threat entries loaded.");
        sLog.outString();
        return;
    }

    SpellRankHelper<SpellThreatEntry, DoSpellThreat, SpellThreatMap> rankHelper(*this, mSpellThreatMap);

    BarGoLink bar(rows.size());

    for (SpellThreatRow const& row : rows)
    {
        bar.step();

        uint32 entry = row.entry;

        SpellThreatEntry ste;
        ste.threat = row.threat;
        ste.multiplier = row.multiplier;
        ste.ap_bonus = row.apBonus;

        rankHelper.RecordRank(ste, entry);
    }

    rankHelper.FillHigherRanks();

    sLog.outString(">> Loaded %u spell threat entries", rankHelper.worker.count);
    sLog.outString();
}

/**
 * @brief Fills the spell script targets from a read that answered rows or an empty table, then checks them.
 */
static void FillSpellScriptTargets(SpellScriptTargetStore& targets, TableRead<SpellScriptTargetRow> const& read)
{
    std::vector<SpellTargetEntry> entries;
    entries.reserve(read.rows.size());
    for (SpellScriptTargetRow const& row : read.rows)
    {
        SpellTargetEntry entry;
        entry.spellId = row.entry;
        entry.type = row.type;
        entry.targetEntry = row.targetEntry;
        entry.inverseEffectMask = row.inverseEffectMask;
        entries.push_back(entry);
    }

    if (read.status == TableReadStatus::Empty)
    {
        sLog.outError("spell_script_target table is empty!\n");
    }
    else
    {
        targets.Clear();
        BarGoLink bar(entries.size());
        for (SpellTargetEntry const& entry : entries)
        {
            bar.step();
            targets.Add(entry.spellId, entry);
        }
    }

    // Check content: every row read, in the order read, the targets of a spell already erased included
    for (std::vector<SpellTargetEntry>::const_iterator itr = entries.begin(); itr != entries.end(); ++itr)
    {
        SpellEntry const* spellProto = sSpellStore.LookupEntry(itr->spellId);
        if (!spellProto)
        {
            sLog.outErrorDb("Table `spell_script_target`: spellId %u listed for TargetEntry %u does not exist.", itr->spellId, itr->targetEntry);
            targets.Erase(itr->spellId);
            continue;
        }

        bool targetfound = false;
        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            SpellEffectEntry const* spellEffect = spellProto->GetSpellEffect(SpellEffectIndex(i));
            if (!spellEffect)
            {
                continue;
            }

            if ( spellEffect->ImplicitTarget_0 == TARGET_SCRIPT ||
                spellEffect->ImplicitTarget_1 == TARGET_SCRIPT ||
                spellEffect->ImplicitTarget_0 == TARGET_SCRIPT_COORDINATES ||
                spellEffect->ImplicitTarget_1 == TARGET_SCRIPT_COORDINATES ||
                spellEffect->ImplicitTarget_0 == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                spellEffect->ImplicitTarget_1 == TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT ||
                spellEffect->ImplicitTarget_0 == TARGET_AREAEFFECT_INSTANT ||
                spellEffect->ImplicitTarget_1 == TARGET_AREAEFFECT_INSTANT ||
                spellEffect->ImplicitTarget_0 == TARGET_AREAEFFECT_CUSTOM ||
                spellEffect->ImplicitTarget_1 == TARGET_AREAEFFECT_CUSTOM ||
                spellEffect->ImplicitTarget_0 == TARGET_AREAEFFECT_GO_AROUND_SOURCE ||
                spellEffect->ImplicitTarget_1 == TARGET_AREAEFFECT_GO_AROUND_SOURCE ||
                spellEffect->ImplicitTarget_0 == TARGET_AREAEFFECT_GO_AROUND_DEST ||
                spellEffect->ImplicitTarget_1 == TARGET_AREAEFFECT_GO_AROUND_DEST ||
                spellEffect->ImplicitTarget_0 == TARGET_NARROW_FRONTAL_CONE ||
                spellEffect->ImplicitTarget_1 == TARGET_NARROW_FRONTAL_CONE ||
                spellEffect->ImplicitTarget_0 == TARGET_NARROW_FRONTAL_CONE_2 ||
                spellEffect->ImplicitTarget_1 == TARGET_NARROW_FRONTAL_CONE_2)
            {
                targetfound = true;
                break;
            }
        }
        if (!targetfound)
        {
            sLog.outErrorDb("Table `spell_script_target`: spellId %u listed for TargetEntry %u does not have any implicit target TARGET_SCRIPT(38) or TARGET_SCRIPT_COORDINATES (46) or TARGET_FOCUS_OR_SCRIPTED_GAMEOBJECT (40).", itr->spellId, itr->targetEntry);
            targets.Erase(itr->spellId);
            continue;
        }

        if (itr->type >= MAX_SPELL_TARGET_TYPE)
        {
            sLog.outErrorDb("Table `spell_script_target`: target type %u for TargetEntry %u is incorrect.", itr->type, itr->targetEntry);
            targets.Erase(itr->spellId);
            continue;
        }

        // Checks by target type
        switch (itr->type)
        {
            case SPELL_TARGET_TYPE_GAMEOBJECT:
            {
                if (!itr->targetEntry)
                {
                    break;
                }

                if (!ObjectMgr::GetGameObjectInfo(itr->targetEntry))
                {
                    sLog.outErrorDb("Table `spell_script_target`: gameobject template entry %u does not exist.", itr->targetEntry);
                    targets.Erase(itr->spellId);
                    continue;
                }
                break;
            }
            default:
                if (!itr->targetEntry)
                {
                    sLog.outErrorDb("Table `spell_script_target`: target entry == 0 for not GO target type (%u).", itr->type);
                    targets.Erase(itr->spellId);
                    continue;
                }
                if (const CreatureInfo* cInfo = ObjectMgr::GetCreatureTemplate(itr->targetEntry))
                {
                    if (itr->spellId == 30427 && !cInfo->SkinningLootId)
                    {
                        sLog.outErrorDb("Table `spell_script_target` has creature %u as a target of spellid 30427, but this creature has no SkinningLootId. Gas extraction will not work!", cInfo->Entry);
                        targets.Erase(itr->spellId);
                        continue;
                    }
                }
                else
                {
                    sLog.outErrorDb("Table `spell_script_target`: creature template entry %u does not exist.", itr->targetEntry);
                    targets.Erase(itr->spellId);
                    continue;
                }
                break;
        }
    }

    // Check all spells
    if (!sLog.HasLogFilter(LOG_FILTER_DB_STRICTED_CHECK))
    {
        for (uint32 i = 1; i < sSpellStore.GetNumRows(); ++i)
        {
            SpellEntry const* spellInfo = sSpellStore.LookupEntry(i);
            if (!spellInfo)
            {
                continue;
            }

            for (int j = 0; j < MAX_EFFECT_INDEX; ++j)
            {
                SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(SpellEffectIndex(j));
                if (!spellEffect)
                {
                    continue;
                }

                if (spellEffect->ImplicitTarget_0 == TARGET_SCRIPT ||
                    spellEffect->ImplicitTarget_0 != TARGET_SELF && spellEffect->ImplicitTarget_1 == TARGET_SCRIPT)
                {
                    SpellScriptTargetStore::Bounds bounds = targets.GetBounds(i);
                    if (bounds.first == bounds.second)
                    {
                        sLog.outErrorDb("Spell (ID: %u) has effect EffectImplicitTargetA/EffectImplicitTargetB = %u (TARGET_SCRIPT), but does not have record in `spell_script_target`", spellInfo->ID, TARGET_SCRIPT);
                        break;                              // effects of spell
                    }
                }
            }
        }
    }
}

/**
 * @brief Loads and validates scripted spell targets; stops the server when the table cannot be read.
 */
void SpellMgr::LoadSpellScriptTarget()
{
    TableRead<SpellScriptTargetRow> read = SpellScriptTargetRepository::QuerySpellScriptTargets();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `spell_script_target` table: %s\n", TableReadReason(read.status));
        Log::WaitBeforeContinueIfNeed();
        exit(1);
    }

    FillSpellScriptTargets(mSpellScriptTargets, read);
}

/**
 * @brief Reloads and validates scripted spell targets; keeps the targets already loaded when the table cannot be read.
 */
TableReadStatus SpellMgr::ReloadSpellScriptTarget()
{
    TableRead<SpellScriptTargetRow> read = SpellScriptTargetRepository::QuerySpellScriptTargets();
    if (read.Unreadable())
    {
        sLog.outError("Error loading `spell_script_target` table: %s; the %u spell script targets already loaded are kept\n", TableReadReason(read.status), mSpellScriptTargets.GetCount());
        return read.status;
    }

    FillSpellScriptTargets(mSpellScriptTargets, read);
    return read.status;
}
