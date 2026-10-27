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
 * @file ListCommands.cpp
 * @brief Implementation of listing and information display chat commands.
 *
 * This file contains chat command handlers for displaying lists including:
 * - Aura listing
 * - Item list display
 * - NPC and creature listing
 * - Quest and achievement lists
 */

#include "Common/Locales.h"
#include <sstream>
#include <string>
#include "Chat.h"
#include "ObjectMgr.h"
#include "Language.h"
#include "spells/auras/SpellAuras.h"
#include "spells/data/SpellMgr.h"
#include "Unit.h"
#include "Player.h"
#include "PlayerRegistry.h"
#include "WorldSession.h"
#include "persistence/characters/CharacterRepository.h"
#include "persistence/characters/inventory/ItemRepository.h"
#include "persistence/world/spawns/SpawnRepository.h"

static uint32 ReputationRankStrIndex[MAX_REPUTATION_RANK] =
{
    LANG_REP_HATED,    LANG_REP_HOSTILE, LANG_REP_UNFRIENDLY, LANG_REP_NEUTRAL,
    LANG_REP_FRIENDLY, LANG_REP_HONORED, LANG_REP_REVERED,    LANG_REP_EXALTED
};

/**
 * @brief Handler for HandleListAurasCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleListAurasCommand(char* /*args*/)
{
    Unit* unit = getSelectedUnit();
    if (!unit)
    {
        SendSysMessage(LANG_SELECT_CHAR_OR_CREATURE);
        SetSentErrorMessage(true);
        return false;
    }

    char const* talentStr = GetMangosString(LANG_TALENT);
    char const* passiveStr = GetMangosString(LANG_PASSIVE);

    Unit::SpellAuraHolderMap const& uAuras = unit->GetSpellAuraHolderMap();
    PSendSysMessage(LANG_COMMAND_TARGET_LISTAURAS, uAuras.size());
    for (Unit::SpellAuraHolderMap::const_iterator itr = uAuras.begin(); itr != uAuras.end(); ++itr)
    {
        bool talent = GetTalentSpellCost(itr->second->GetId()) > 0;

        SpellAuraHolder* holder = itr->second;
        char const* name = holder->GetSpellProto()->Name_lang ? holder->GetSpellProto()->Name_lang[GetSessionDbcLocale()] : "";

        for (int32 i = 0; i < MAX_EFFECT_INDEX; ++i)
        {
            Aura* aur = holder->GetAuraByEffectIndex(SpellEffectIndex(i));
            if (!aur)
            {
                continue;
            }

            if (m_session)
            {
                std::ostringstream ss_name;
                ss_name << "|cffffffff|Hspell:" << itr->second->GetId() << "|h[" << name << "]|h|r";

                PSendSysMessage(LANG_COMMAND_TARGET_AURADETAIL, holder->GetId(), aur->GetEffIndex(),
                                aur->GetModifier()->m_auraname, aur->GetAuraDuration(), aur->GetAuraMaxDuration(),
                                ss_name.str().c_str(),
                                (holder->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                holder->GetCasterGuid().GetString().c_str());
            }
            else
            {
                PSendSysMessage(LANG_COMMAND_TARGET_AURADETAIL, holder->GetId(), aur->GetEffIndex(),
                                aur->GetModifier()->m_auraname, aur->GetAuraDuration(), aur->GetAuraMaxDuration(),
                                name,
                                (holder->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                holder->GetCasterGuid().GetString().c_str());
            }
        }
    }
    for (int i = 0; i < TOTAL_AURAS; ++i)
    {
        Unit::AuraList const& uAuraList = unit->GetAurasByType(AuraType(i));
        if (uAuraList.empty())
        {
            continue;
        }
        PSendSysMessage(LANG_COMMAND_TARGET_LISTAURATYPE, uAuraList.size(), i);
        for (Unit::AuraList::const_iterator itr = uAuraList.begin(); itr != uAuraList.end(); ++itr)
        {
            bool talent = GetTalentSpellCost((*itr)->GetId()) > 0;

            char const* name = (*itr)->GetSpellProto()->Name_lang ? (*itr)->GetSpellProto()->Name_lang[GetSessionDbcLocale()] : "";

            if (m_session)
            {
                std::ostringstream ss_name;
                ss_name << "|cffffffff|Hspell:" << (*itr)->GetId() << "|h[" << name << "]|h|r";

                PSendSysMessage(LANG_COMMAND_TARGET_AURASIMPLE, (*itr)->GetId(), (*itr)->GetEffIndex(),
                                ss_name.str().c_str(), ((*itr)->GetHolder()->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                (*itr)->GetCasterGuid().GetString().c_str());
            }
            else
            {
                PSendSysMessage(LANG_COMMAND_TARGET_AURASIMPLE, (*itr)->GetId(), (*itr)->GetEffIndex(),
                                name, ((*itr)->GetHolder()->IsPassive() ? passiveStr : ""), (talent ? talentStr : ""),
                                (*itr)->GetCasterGuid().GetString().c_str());
            }
        }
    }
    return true;
}

/**
 * @brief Handler for HandleListTalentsCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleListTalentsCommand(char* /*args*/)
{
    Player* player = getSelectedPlayer();
    if (!player)
    {
        SendSysMessage(LANG_NO_CHAR_SELECTED);
        SetSentErrorMessage(true);
        return false;
    }

    SendSysMessage(LANG_LIST_TALENTS_TITLE);
    uint32 count = 0;
    uint32 cost = 0;
    PlayerSpellMap const& uSpells = player->GetSpellMap();
    for (PlayerSpellMap::const_iterator itr = uSpells.begin(); itr != uSpells.end(); ++itr)
    {
        if (itr->second.state == PLAYERSPELL_REMOVED || itr->second.disabled)
        {
            continue;
        }

        uint32 cost_itr = GetTalentSpellCost(itr->first);

        if (cost_itr == 0)
        {
            continue;
        }

        SpellEntry const* spellEntry = sSpellStore.LookupEntry(itr->first);
        if (!spellEntry)
        {
            continue;
        }

        ShowSpellListHelper(player, spellEntry, GetSessionDbcLocale());
        ++count;
        cost += cost_itr;
    }
    PSendSysMessage(LANG_LIST_TALENTS_COUNT, count, cost);

    return true;
}

/**
 * @brief Handler for HandleListItemCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleListItemCommand(char* args)
{
    uint32 item_id;
    if (!ExtractUint32KeyFromLink(&args, "Hitem", item_id))
    {
        return false;
    }

    if (!item_id)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    ItemPrototype const* itemProto = ObjectMgr::GetItemPrototype(item_id);
    if (!itemProto)
    {
        PSendSysMessage(LANG_COMMAND_ITEMIDINVALID, item_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
    {
        return false;
    }

    // inventory case
    uint32 inv_count = 0;
    if (std::optional<uint32> invCount = ItemRepository::QueryInventoryCountOf(item_id))
    {
        inv_count = *invCount;
    }

    std::vector<ItemInventoryHolderRow> invHolders = ItemRepository::QueryInventoryHoldersOf(item_id, uint32(count));

    if (!invHolders.empty())
    {
        for (ItemInventoryHolderRow const& row : invHolders)
        {
            uint32 item_guid = row.itemGuid;
            uint32 item_bag = row.bag;
            uint32 item_slot = row.slot;
            uint32 owner_guid = row.ownerGuid;
            uint32 owner_acc = row.ownerAccount;
            std::string owner_name = row.ownerName;

            char const* item_pos = 0;
            if (InventoryMgr::IsEquipmentPos(item_bag, item_slot))
            {
                item_pos = "[equipped]";
            }
            else if (InventoryMgr::IsInventoryPos(item_bag, item_slot))
            {
                item_pos = "[in inventory]";
            }
            else if (InventoryMgr::IsBankPos(item_bag, item_slot))
            {
                item_pos = "[in bank]";
            }
            else
            {
                item_pos = "";
            }

            PSendSysMessage(LANG_ITEMLIST_SLOT,
                            item_guid, owner_name.c_str(), owner_guid, owner_acc, item_pos);
        }

        uint32 res_count = uint32(invHolders.size());

        if (count > res_count)
        {
            count -= res_count;
        }
        else if (count)
        {
            count = 0;
        }
    }

    // mail case
    uint32 mail_count = 0;
    if (std::optional<uint32> mailCount = ItemRepository::QueryMailCountOf(item_id))
    {
        mail_count = *mailCount;
    }

    std::vector<ItemMailHolderRow> mailHolders;
    if (count > 0)
    {
        mailHolders = ItemRepository::QueryMailHoldersOf(item_id, uint32(count));
    }

    if (!mailHolders.empty())
    {
        for (ItemMailHolderRow const& row : mailHolders)
        {
            uint32 item_guid = row.itemGuid;
            uint32 item_s = row.sender;
            uint32 item_r = row.receiver;
            uint32 item_s_acc = row.senderAccount;
            std::string item_s_name = row.senderName;
            uint32 item_r_acc = row.receiverAccount;
            std::string item_r_name = row.receiverName;

            char const* item_pos = "[in mail]";

            PSendSysMessage(LANG_ITEMLIST_MAIL,
                            item_guid, item_s_name.c_str(), item_s, item_s_acc, item_r_name.c_str(), item_r, item_r_acc, item_pos);
        }

        uint32 res_count = uint32(mailHolders.size());

        if (count > res_count)
        {
            count -= res_count;
        }
        else if (count)
        {
            count = 0;
        }
    }

    // auction case
    uint32 auc_count = 0;
    if (std::optional<uint32> aucCount = ItemRepository::QueryAuctionCountOf(item_id))
    {
        auc_count = *aucCount;
    }

    std::vector<ItemAuctionHolderRow> aucHolders;
    if (count > 0)
    {
        aucHolders = ItemRepository::QueryAuctionHoldersOf(item_id, uint32(count));
    }

    if (!aucHolders.empty())
    {
        for (ItemAuctionHolderRow const& row : aucHolders)
        {
            uint32 item_guid = row.itemGuid;
            uint32 owner = row.ownerGuid;
            uint32 owner_acc = row.ownerAccount;
            std::string owner_name = row.ownerName;

            char const* item_pos = "[in auction]";

            PSendSysMessage(LANG_ITEMLIST_AUCTION, item_guid, owner_name.c_str(), owner, owner_acc, item_pos);
        }
    }

    // guild bank case
    uint32 guild_count = 0;
    if (std::optional<uint32> guildCount = ItemRepository::QueryGuildBankCountOf(item_id))
    {
        guild_count = *guildCount;
    }

    std::vector<ItemGuildBankHolderRow> guildHolders = ItemRepository::QueryGuildBankHoldersOf(item_id, uint32(count));

    if (!guildHolders.empty())
    {
        for (ItemGuildBankHolderRow const& row : guildHolders)
        {
            uint32 item_guid = row.itemGuid;
            uint32 guild_guid = row.guildId;
            std::string guild_name = row.guildName;

            char const* item_pos = "[in guild bank]";

            PSendSysMessage(LANG_ITEMLIST_GUILD, item_guid, guild_name.c_str(), guild_guid, item_pos);
        }

        uint32 res_count = uint32(guildHolders.size());

        if (count > res_count)
        {
            count -= res_count;
        }
        else if (count)
        {
            count = 0;
        }
    }

    if (inv_count + mail_count + auc_count + guild_count == 0)
    {
        SendSysMessage(LANG_COMMAND_NOITEMFOUND);
        SetSentErrorMessage(true);
        return false;
    }

    PSendSysMessage(LANG_COMMAND_LISTITEMMESSAGE, item_id, inv_count + mail_count + auc_count + guild_count, inv_count, mail_count, auc_count, guild_count);

    return true;
}

///**
// * @brief Handler for HandleListPlayersCommand command.
// *
// * @param args Command arguments.
// * @returns True if the command executed successfully, false otherwise.
// */
//bool ChatHandler::HandleListPlayersCommand(char* args)
//{
//    uint32 limit;
//    if (!ExtractOptUInt32(&args, limit, 100))
//    {
//        return false;
//    }
//
//    uint32 count = 0;
//
//    PSendSysMessage("Online Players (Limit %u):", limit);
//    PSendSysMessage("===========================================");
//
//    sPlayerRegistry.ForEach([&](Player* player)
//    {
//        if (count >= limit)
//        {
//            return;
//        }
//
//        uint32 mapId = player->GetMapId();
//        uint32 zoneId = player->GetTerrain()->GetZoneId(player->Where().X(), player->Where().Y(), player->Where().Z());
//
//        MapEntry const* mapEntry = sMapStore.LookupEntry(mapId);
//        AreaTableEntry const* zoneEntry = GetAreaEntryByAreaID(zoneId);
//
//        PSendSysMessage("%-20s | Lvl %-2u | Map %u Zone %u (%s)",
//                       player->GetName(),
//                       player->getLevel(),
//                       mapId,
//                       zoneId,
//                       (zoneEntry ? zoneEntry->area_name[GetSessionDbcLocale()] : "Unknown"));
//
//        count++;
//    });
//
//    return true;
//}

/**
 * @brief Handler for HandleListObjectCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleListObjectCommand(char* args)
{
    // number or [name] Shift-click form |color|Hgameobject_entry:go_id|h[name]|h|r
    uint32 go_id;
    if (!ExtractUint32KeyFromLink(&args, "Hgameobject_entry", go_id))
    {
        return false;
    }

    if (!go_id)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    GameObjectInfo const* gInfo = ObjectMgr::GetGameObjectInfo(go_id);
    if (!gInfo)
    {
        PSendSysMessage(LANG_COMMAND_LISTOBJINVALIDID, go_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
    {
        return false;
    }

    uint32 obj_count = 0;
    if (std::optional<uint32> spawnCount = SpawnRepository::QueryGameObjectCountOf(go_id))
    {
        obj_count = *spawnCount;
    }

    std::vector<SpawnListRow> spawns;
    if (m_session)
    {
        Player* pl = m_session->GetPlayer();
        spawns = SpawnRepository::QueryGameObjectsNear(pl->Where().X(), pl->Where().Y(), pl->Where().Z(), go_id, uint32(count));
    }
    else
        spawns = SpawnRepository::QueryGameObjectsOf(go_id, uint32(count));

    if (!spawns.empty())
    {
        for (SpawnListRow const& row : spawns)
        {
            uint32 guid = row.guid;
            float x = row.positionX;
            float y = row.positionY;
            float z = row.positionZ;
            int mapid = row.map;

            if (m_session)
            {
                PSendSysMessage(LANG_GO_LIST_CHAT, guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(guid).c_str(), guid, gInfo->name, x, y, z, mapid);
            }
            else
            {
                PSendSysMessage(LANG_GO_LIST_CONSOLE, guid, PrepareStringNpcOrGoSpawnInformation<GameObject>(guid).c_str(), gInfo->name, x, y, z, mapid);
            }
        }
    }

    PSendSysMessage(LANG_COMMAND_LISTOBJMESSAGE, go_id, obj_count);
    return true;
}

/**
 * @brief Handler for HandleListCreatureCommand command.
 *
 * @param args Command arguments.
 * @returns True if the command executed successfully, false otherwise.
 */
bool ChatHandler::HandleListCreatureCommand(char* args)
{
    // number or [name] Shift-click form |color|Hcreature_entry:creature_id|h[name]|h|r
    uint32 cr_id;
    if (!ExtractUint32KeyFromLink(&args, "Hcreature_entry", cr_id))
    {
        return false;
    }

    if (!cr_id)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    CreatureInfo const* cInfo = ObjectMgr::GetCreatureTemplate(cr_id);
    if (!cInfo)
    {
        PSendSysMessage(LANG_COMMAND_INVALIDCREATUREID, cr_id);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 count;
    if (!ExtractOptUInt32(&args, count, 10))
    {
        return false;
    }

    uint32 cr_count = 0;
    if (std::optional<uint32> spawnCount = SpawnRepository::QueryCreatureCountOf(cr_id))
    {
        cr_count = *spawnCount;
    }

    std::vector<SpawnListRow> spawns;
    if (m_session)
    {
        Player* pl = m_session->GetPlayer();
        spawns = SpawnRepository::QueryCreaturesNear(pl->Where().X(), pl->Where().Y(), pl->Where().Z(), cr_id, uint32(count));
    }
    else
        spawns = SpawnRepository::QueryCreaturesOf(cr_id, uint32(count));

    if (!spawns.empty())
    {
        for (SpawnListRow const& row : spawns)
        {
            uint32 guid = row.guid;
            float x = row.positionX;
            float y = row.positionY;
            float z = row.positionZ;
            int mapid = row.map;

            if (m_session)
            {
                PSendSysMessage(LANG_CREATURE_LIST_CHAT, guid, PrepareStringNpcOrGoSpawnInformation<Creature>(guid).c_str(), guid, cInfo->Name, x, y, z, mapid);
            }
            else
            {
                PSendSysMessage(LANG_CREATURE_LIST_CONSOLE, guid, PrepareStringNpcOrGoSpawnInformation<Creature>(guid).c_str(), cInfo->Name, x, y, z, mapid);
            }
        }
    }

    PSendSysMessage(LANG_COMMAND_LISTCREATUREMESSAGE, cr_id, cr_count);
    return true;
}

void ChatHandler::ShowSpellListHelper(Player* target, SpellEntry const* spellInfo, LocaleConstant loc)
{
    uint32 id = spellInfo->ID;

    bool known = target && target->HasSpell(id);
    SpellEffectEntry const* spellEffect = spellInfo->GetSpellEffect(EFFECT_INDEX_0);
    bool learn = (spellEffect && spellEffect->Effect == SPELL_EFFECT_LEARN_SPELL);

    uint32 talentCost = GetTalentSpellCost(id);

    bool talent = (talentCost > 0);
    bool passive = IsPassiveSpell(spellInfo);
    bool active = target && target->HasAura(id);

    // unit32 used to prevent interpreting uint8 as char at output
    // find rank of learned spell for learning spell, or talent rank
    uint32 rank = talentCost ? talentCost : sSpellMgr.GetSpellRank(learn ? (spellEffect ? spellEffect->EffectTriggerSpell : 0) : id);

    // send spell in "id - [name, rank N] [talent] [passive] [learn] [known]" format
    std::ostringstream ss;
    char const* nameStr = (spellInfo->Name_lang && spellInfo->Name_lang[loc]) ? spellInfo->Name_lang[loc] : "";
    if (m_session)
    {
        ss << id << " - |cffffffff|Hspell:" << id << "|h[" << nameStr;
    }
    else
    {
        ss << id << " - " << nameStr;
    }

    // include the spell's subtext ("Rank N" / label) to distinguish same-named
    // spells; fall back to the computed numeric rank when there is no subtext
    char const* subText = spellInfo->NameSubtext_lang ? spellInfo->NameSubtext_lang[loc] : NULL;
    if (subText && *subText)
    {
        ss << " " << subText;
    }
    else if (rank)
    {
        ss << GetMangosString(LANG_SPELL_RANK) << rank;
    }

    if (m_session)
    {
        ss << " " << localeNames[loc] << "]|h|r";
    }
    else
    {
        ss << " " << localeNames[loc];
    }

    if (talent)
    {
        ss << GetMangosString(LANG_TALENT);
    }
    if (passive)
    {
        ss << GetMangosString(LANG_PASSIVE);
    }
    if (learn)
    {
        ss << GetMangosString(LANG_LEARN);
    }
    if (known)
    {
        ss << GetMangosString(LANG_KNOWN);
    }
    if (active)
    {
        ss << GetMangosString(LANG_ACTIVE);
    }

    SendSysMessage(ss.str().c_str());
}

void ChatHandler::ShowCurrencyListHelper(Player* target, CurrencyTypesEntry const* currency, LocaleConstant loc)
{
    uint32 id = currency->ID;

    uint32 count = target ? target->GetCurrencyMgr().GetCount(id) : 0;

    uint32 talentCost = GetTalentSpellCost(id);

    // send spell in "id - [name] (Amount: x)" format
    std::ostringstream ss;
    if (m_session)
    {
        ss << id << " - |cff00aa00|Hcurrency:" << id << "|h[" << currency->Name_lang[loc];
    }
    else
    {
        ss << id << " - " << currency->Name_lang[loc];
    }

    if (m_session)
    {
        ss << " " << localeNames[loc] << "]|h|r";
    }
    else
    {
        ss << " " << localeNames[loc];
    }

    if (target)
    {
        ss << " (" << GetMangosString(LANG_CURRENCY_AMOUNT) << ": " << count << ")";
    }

    SendSysMessage(ss.str().c_str());
}

bool ChatHandler::ShowPlayerListHelper(std::vector<AccountCharacterListRow> const& rows, uint32* limit, bool title, bool error)
{
    if (rows.empty())
    {
        if (error)
        {
            PSendSysMessage(LANG_NO_PLAYERS_FOUND);
            SetSentErrorMessage(true);
        }
        return false;
    }

    if (!m_session && title)
    {
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
        SendSysMessage(LANG_CHARACTERS_LIST_HEADER);
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
    }

    if (!rows.empty())
    {
        ///- Circle through them. Display username and GM level
        for (AccountCharacterListRow const& row : rows)
        {
            // check limit
            if (limit)
            {
                if (*limit == 0)
                {
                    break;
                }
                --* limit;
            }

            uint32 guid = row.guid;
            std::string name = row.name;
            uint8 race = row.race;
            uint8 class_ = row.classId;
            uint32 level = row.level;

            ChrRacesEntry const* raceEntry = sChrRacesStore.LookupEntry(race);
            ChrClassesEntry const* classEntry = sChrClassesStore.LookupEntry(class_);

            char const* race_name = raceEntry ? raceEntry->Name_lang[GetSessionDbcLocale()] : "<?>";
            char const* class_name = classEntry ? classEntry->Name_lang[GetSessionDbcLocale()] : "<?>";

            if (!m_session)
            {
                PSendSysMessage(LANG_CHARACTERS_LIST_LINE_CONSOLE, guid, name.c_str(), race_name, class_name, level);
            }
            else
            {
                PSendSysMessage(LANG_CHARACTERS_LIST_LINE_CHAT, guid, name.c_str(), name.c_str(), race_name, class_name, level);
            }
        }
    }

    if (!m_session)
    {
        SendSysMessage(LANG_CHARACTERS_LIST_BAR);
    }

    return true;
}

void ChatHandler::ShowFactionListHelper(FactionEntry const* factionEntry, LocaleConstant loc, FactionState const* repState /*= NULL*/, Player* target /*= NULL */)
{
    std::string name = factionEntry->Name_lang[loc];

    // send faction in "id - [faction] rank reputation [visible] [at war] [own team] [unknown] [invisible] [inactive]" format
    // or              "id - [faction] [no reputation]" format
    std::ostringstream ss;
    if (m_session)
    {
        ss << factionEntry->ID << " - |cffffffff|Hfaction:" << factionEntry->ID << "|h[" << name << " " << localeNames[loc] << "]|h|r";
    }
    else
    {
        ss << factionEntry->ID << " - " << name << " " << localeNames[loc];
    }

    if (repState)                               // and then target!=NULL also
    {
        ReputationRank rank = target->GetReputationMgr().GetRank(factionEntry);
        std::string rankName = GetMangosString(ReputationRankStrIndex[rank]);

        ss << " " << rankName << "|h|r (" << target->GetReputationMgr().GetReputation(factionEntry) << ")";

        if (repState->Flags & FACTION_FLAG_VISIBLE)
        {
            ss << GetMangosString(LANG_FACTION_VISIBLE);
        }
        if (repState->Flags & FACTION_FLAG_AT_WAR)
        {
            ss << GetMangosString(LANG_FACTION_ATWAR);
        }
        if (repState->Flags & FACTION_FLAG_PEACE_FORCED)
        {
            ss << GetMangosString(LANG_FACTION_PEACE_FORCED);
        }
        if (repState->Flags & FACTION_FLAG_HIDDEN)
        {
            ss << GetMangosString(LANG_FACTION_HIDDEN);
        }
        if (repState->Flags & FACTION_FLAG_INVISIBLE_FORCED)
        {
            ss << GetMangosString(LANG_FACTION_INVISIBLE_FORCED);
        }
        if (repState->Flags & FACTION_FLAG_INACTIVE)
        {
            ss << GetMangosString(LANG_FACTION_INACTIVE);
        }
    }
    else if (target)
    {
        ss << GetMangosString(LANG_FACTION_NOREPUTATION);
    }

    SendSysMessage(ss.str().c_str());
}