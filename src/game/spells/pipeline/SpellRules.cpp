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

#include "spells/pipeline/SpellRules.h"

#include "DBCStores.h"
#include "Unit.h"
#include "spells/SpellModMgr.h"
#include "spells/handlers/SpellMgrHandlers.h"
#include "spells/pipeline/Spell.h"

/**
 * @brief Calculates the effective spell duration for a caster.
 *
 * @param spellInfo The spell entry.
 * @param caster The unit casting the spell.
 * @return The adjusted duration in milliseconds.
 */
int32 CalculateSpellDuration(SpellEntry const* spellInfo, Unit const* caster)
{
    int32 duration = GetSpellDuration(spellInfo);

    if (duration != -1 && caster)
    {
        int32 maxduration = GetSpellMaxDuration(spellInfo);

        Player const* casterPlayer = caster->ToPlayer();
        if (duration != maxduration && casterPlayer)
        {
            duration += int32((maxduration - duration) * casterPlayer->GetComboPoints() / 5);
        }

        if (SpellModMgr* modOwner = caster->GetSpellMods())
        {
            modOwner->ApplySpellMod(spellInfo->ID, SPELLMOD_DURATION, duration);

            if (duration < 0)
            {
                duration = 0;
            }
        }
    }

    return duration;
}

/**
 * @brief Returns the effective cast time of a spell.
 *
 * @param spellInfo The spell entry.
 * @param spell The spell instance, if available.
 * @return The cast time in milliseconds.
 */
uint32 GetSpellCastTime(SpellEntry const* spellInfo, Spell const* spell)
{
    if (!spell)
    {
        return GetSpellCastTime(spellInfo);
    }

    // some triggered spells have data only usable for client
    if (spell->IsTriggeredSpellWithRedundentCastTime())
    {
        return 0;
    }

    // spell targeted to non-trading trade slot item instant at trade success apply
    if (Player* casterPlayer = spell->GetCaster()->ToPlayer())
        if (TradeData* my_trade = casterPlayer->GetTradeData())
            if (Item* nonTrade = my_trade->GetTraderData()->GetItem(TRADE_SLOT_NONTRADED))
                if (nonTrade == spell->m_targets.getItemTarget())
                {
                    return 0;
                }

    uint32 castTime = 0;
    SpellScalingEntry const* spellScalingEntry = spellInfo->GetSpellScaling();
    if (spellScalingEntry && (spell->GetCaster()->GetTypeId() == TYPEID_PLAYER || spell->GetCaster()->GetObjectGuid().IsPet()))
    {
        uint32 level = spell->GetCaster()->getLevel();
        if (level == 1)
        {
            castTime = uint32(spellScalingEntry->CastTimeMin);
        }
        else if (level < uint32(spellScalingEntry->CastTimeMaxLevel))
            castTime = uint32(spellScalingEntry->CastTimeMin + float(level - 1) *
                (spellScalingEntry->CastTimeMax - spellScalingEntry->CastTimeMin) / (spellScalingEntry->CastTimeMaxLevel - 1));
        else
        {
            castTime = uint32(spellScalingEntry->CastTimeMax);
        }
    }
    else if (SpellCastTimesEntry const* spellCastTimeEntry = sSpellCastTimesStore.LookupEntry(spellInfo->CastingTimeIndex))
    {
        uint32 level = spell->GetCaster()->getLevel();
        if (SpellLevelsEntry const* levelsEntry = spellInfo->GetSpellLevels())
        {
            if (levelsEntry->MaxLevel)
            {
                level = std::min(level, levelsEntry->MaxLevel);
            }
            level = std::max(level, levelsEntry->BaseLevel) - levelsEntry->BaseLevel;
        }

        // currently only profession spells have CastTimePerLevel data filled, always negative
        castTime = uint32(spellCastTimeEntry->CastTime + spellCastTimeEntry->CastTimePerLevel * level);

        if (castTime < uint32(spellCastTimeEntry->MinCastTime))
        {
            castTime = uint32(spellCastTimeEntry->MinCastTime);
        }
    }
    else
        // not all spells have cast time index and this is all is pasiive abilities
        return 0;

    if (SpellModMgr* modOwner = spell->GetCaster()->GetSpellMods())
    {
        modOwner->ApplySpellMod(spellInfo->ID, SPELLMOD_CASTING_TIME, castTime);
    }

    if (!spellInfo->HasAttribute(SPELL_ATTR_ABILITY) && !spellInfo->HasAttribute(SPELL_ATTR_TRADESPELL))
    {
        castTime = uint32(castTime * spell->GetCaster()->GetFloatValue(UNIT_MOD_CAST_SPEED));
    }
    else
    {
        if (spell->IsRangedSpell() && !spell->IsAutoRepeat())
        {
            castTime = uint32(castTime * spell->GetCaster()->m_modAttackSpeedPct[RANGED_ATTACK]);
        }
    }

    if (spellInfo->HasAttribute(SPELL_ATTR_RANGED) && !spell->IsAutoRepeat())
    {
        castTime += 500;
    }

    return (castTime > 0) ? uint32(castTime) : 0;
}

/**
 * @brief Checks whether a spell is allowed in a given location.
 *
 * @param spellInfo The spell entry to validate.
 * @param map_id The current map identifier.
 * @param zone_id The current zone identifier.
 * @param area_id The current area identifier.
 * @param player The player attempting the cast, if any.
 * @return The spell cast failure code, or SPELL_CAST_OK when allowed.
 */
SpellCastResult GetSpellAllowedInLocationError(SpellEntry const* spellInfo, uint32 map_id, uint32 zone_id, uint32 area_id, Player const* player,
                                               SpellAreaConditionCheck const& meetsCondition)
{
    // normal case
    int32 areaGroupId = spellInfo->GetAreaGroupId();
    if (areaGroupId > 0)
    {
        bool found = false;
        AreaGroupEntry const* groupEntry = sAreaGroupStore.LookupEntry(areaGroupId);
        while (groupEntry)
        {
            for (uint32 i = 0; i < 6; ++i)
                if (groupEntry->AreaID[i] == zone_id || groupEntry->AreaID[i] == area_id)
                {
                    found = true;
                }
            if (found || !groupEntry->NextAreaID)
            {
                break;
            }
            // Try search in next group
            groupEntry = sAreaGroupStore.LookupEntry(groupEntry->NextAreaID);
        }

        if (!found)
        {
            return SPELL_FAILED_INCORRECT_AREA;
        }
    }

    // continent limitation (virtual continent), ignore for GM
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_CAST_ONLY_IN_OUTLAND) && !(player && player->isGameMaster()))
    {
        uint32 v_map = GetVirtualMapForMapAndZone(map_id, zone_id);
        MapEntry const* mapEntry = sMapStore.LookupEntry(v_map);
        if (!mapEntry || mapEntry->ExpansionID < 1 || !mapEntry->IsContinent())
        {
            return SPELL_FAILED_INCORRECT_AREA;
        }
    }

    // raid instance limitation
    if (spellInfo->HasAttribute(SPELL_ATTR_EX6_NOT_IN_RAID_INSTANCE))
    {
        MapEntry const* mapEntry = sMapStore.LookupEntry(map_id);
        if (!mapEntry || mapEntry->IsRaid())
        {
            return SPELL_FAILED_NOT_IN_RAID_INSTANCE;
        }
    }

    // DB base check (if non empty then must fit at least single for allow)
    SpellAreaMapBounds saBounds = sSpellMgr.GetSpellAreaMapBounds(spellInfo->ID);
    if (saBounds.first != saBounds.second)
    {
        for (SpellAreaMap::const_iterator itr = saBounds.first; itr != saBounds.second; ++itr)
        {
            if (IsFitToRequirements(itr->second, player, zone_id, area_id, meetsCondition))
            {
                return SPELL_CAST_OK;
            }
        }
        return SPELL_FAILED_INCORRECT_AREA;
    }

    // bg spell checks

    // do not allow spells to be cast in arenas
    // - with SPELL_ATTR_EX4_NOT_USABLE_IN_ARENA flag
    // - with greater than 10 min CD
    if (spellInfo->HasAttribute(SPELL_ATTR_EX4_NOT_USABLE_IN_ARENA) ||
            (GetSpellRecoveryTime(spellInfo) > 10 * MINUTE * IN_MILLISECONDS && !spellInfo->HasAttribute(SPELL_ATTR_EX4_USABLE_IN_ARENA)))
        if (player && player->InArena())
        {
            return SPELL_FAILED_NOT_IN_ARENA;
        }

    // Spell casted only on battleground
    if (spellInfo->HasAttribute(SPELL_ATTR_EX3_BATTLEGROUND))
        if (!player || !player->InBattleGround())
        {
            return SPELL_FAILED_ONLY_BATTLEGROUNDS;
        }

    SpellAllowedInLocationContext ctx(map_id, player);
    SpellHandlerOutcome<SpellCastResult> outcome = SpellHandlerRegistry::Game().Dispatch<SpellAllowedInLocationSite>(spellInfo->ID, ctx);
    if (outcome.IsReturn())
    {
        return outcome.GetValue();
    }

    return SPELL_CAST_OK;
}

/**
 * @brief Checks whether a player satisfies a spell-area requirement record.
 *
 * @param spellArea The requirement record.
 * @param player The player being evaluated.
 * @param newZone The current zone identifier.
 * @param newArea The current area identifier.
 * @param meetsCondition Whether the player meets a condition.
 * @return true if all requirements are met; otherwise, false.
 */
bool IsFitToRequirements(SpellArea const& spellArea, Player const* player, uint32 newZone, uint32 newArea, SpellAreaConditionCheck const& meetsCondition)
{
    if (spellArea.conditionId)
    {
        if (!player || !meetsCondition(spellArea.conditionId))
        {
            return false;
        }
    }
    else
    {
        if (spellArea.gender != GENDER_NONE)
        {
            // not in expected gender
            if (!player || spellArea.gender != player->getGender())
            {
                return false;
            }
        }

        if (spellArea.raceMask)
        {
            // not in expected race
            if (!player || !(spellArea.raceMask & player->getRaceMask()))
            {
                return false;
            }
        }

        if (spellArea.questStart)
        {
            // not in expected required quest state
            if (!player || (!spellArea.questStartCanActive || !player->GetQuestStatusMgr().IsActiveQuest(spellArea.questStart)) && !player->GetQuestRewardStatus(spellArea.questStart))
            {
                return false;
            }
        }

        if (spellArea.questEnd)
        {
            // not in expected forbidden quest state
            if (!player || player->GetQuestRewardStatus(spellArea.questEnd))
            {
                return false;
            }
        }
    }

    if (spellArea.areaId)
    {
        // not in expected zone
        if (newZone != spellArea.areaId && newArea != spellArea.areaId)
        {
            return false;
        }
    }

    if (spellArea.auraSpell)
    {
        // not have expected aura
        if (!player)
        {
            return false;
        }
        if (spellArea.auraSpell > 0)
            // have expected aura
        {
            return player->HasAura(spellArea.auraSpell);
        }
        else
            // not have expected aura
        {
            return !player->HasAura(-spellArea.auraSpell);
        }
    }

    return true;
}

/**
 * @brief Applies or removes an area-based spell according to requirements.
 *
 * @param spellArea The requirement record.
 * @param player The player to update.
 * @param newZone The current zone identifier.
 * @param newArea The current area identifier.
 * @param onlyApply true to skip aura removal when requirements fail.
 * @param meetsCondition Whether the player meets a condition.
 */
void ApplyOrRemoveSpellIfCan(SpellArea const& spellArea, Player* player, uint32 newZone, uint32 newArea, bool onlyApply, SpellAreaConditionCheck const& meetsCondition)
{
    MANGOS_ASSERT(player);

    if (IsFitToRequirements(spellArea, player, newZone, newArea, meetsCondition))
    {
        if (spellArea.autocast && !player->HasAura(spellArea.spellId))
        {
            player->CastSpell(player, spellArea.spellId, true);
        }
    }
    else if (!onlyApply && player->HasAura(spellArea.spellId))
    {
        player->RemoveAurasDueToSpell(spellArea.spellId);
    }
}
