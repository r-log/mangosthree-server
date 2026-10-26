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

#ifndef MANGOS_H_CHARACTERREPOSITORY
#define MANGOS_H_CHARACTERREPOSITORY

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Platform/Define.h"
#include "persistence/characters/inventory/ItemRepository.h"

class QueryResult;
class SqlQueryHolder;

/// The slots of the login holder, one per table the character's load reads.
enum PlayerLoginQueryIndex
{
    PLAYER_LOGIN_QUERY_LOADFROM,
    PLAYER_LOGIN_QUERY_LOADGROUP,
    PLAYER_LOGIN_QUERY_LOADBOUNDINSTANCES,
    PLAYER_LOGIN_QUERY_LOADAURAS,
    PLAYER_LOGIN_QUERY_LOADSPELLS,
    PLAYER_LOGIN_QUERY_LOADQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADDAILYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADREPUTATION,
    PLAYER_LOGIN_QUERY_LOADINVENTORY,
    PLAYER_LOGIN_QUERY_LOADITEMLOOT,
    PLAYER_LOGIN_QUERY_LOADACTIONS,
    PLAYER_LOGIN_QUERY_LOADSOCIALLIST,
    PLAYER_LOGIN_QUERY_LOADHOMEBIND,
    PLAYER_LOGIN_QUERY_LOADSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADDECLINEDNAMES,
    PLAYER_LOGIN_QUERY_LOADGUILD,
    PLAYER_LOGIN_QUERY_LOADARENAINFO,
    PLAYER_LOGIN_QUERY_LOADACHIEVEMENTS,
    PLAYER_LOGIN_QUERY_LOADCRITERIAPROGRESS,
    PLAYER_LOGIN_QUERY_LOADEQUIPMENTSETS,
    PLAYER_LOGIN_QUERY_LOADBGDATA,
    PLAYER_LOGIN_QUERY_LOADACCOUNTDATA,
    PLAYER_LOGIN_QUERY_LOADSKILLS,
    PLAYER_LOGIN_QUERY_LOADGLYPHS,
    PLAYER_LOGIN_QUERY_LOADMAILS,
    PLAYER_LOGIN_QUERY_LOADMAILEDITEMS,
    PLAYER_LOGIN_QUERY_LOADTALENTS,
    PLAYER_LOGIN_QUERY_LOADWEEKLYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADMONTHLYQUESTSTATUS,
    PLAYER_LOGIN_QUERY_LOADCURRENCIES,
    PLAYER_LOGIN_QUERY_LOADCUFPROFILES,
    PLAYER_LOGIN_QUERY_LOADPETS,
    PLAYER_LOGIN_QUERY_LOADPETAURAS,
    PLAYER_LOGIN_QUERY_LOADPETSPELLS,
    PLAYER_LOGIN_QUERY_LOADPETSPELLCOOLDOWNS,
    PLAYER_LOGIN_QUERY_LOADPETDECLINEDNAMES,

    MAX_PLAYER_LOGIN_QUERY
};

/// The slot of the create request's login database holder: how many characters the account has
/// across every realm.
enum CharCreateAccountSlot
{
    CHAR_CREATE_ACCOUNT_REALM_CHARS = 0,
    CHAR_CREATE_ACCOUNT_COUNT       = 1
};

/// The slots of the create request's character database holder: the account's character count on
/// this realm, the account's characters (level, race, class) and the highest pet id.
enum CharCreateRealmSlot
{
    CHAR_CREATE_REALM_CHAR_COUNT    = 0,
    CHAR_CREATE_REALM_ACCOUNT_CHARS = 1,
    CHAR_CREATE_REALM_PET_ID        = 2,
    CHAR_CREATE_REALM_COUNT         = 3
};

/// The slots of the customize request's holder: the character's at_login flags and its
/// playerBytes2.
enum CharCustomizeSlot
{
    CHAR_CUSTOMIZE_AT_LOGIN      = 0,
    CHAR_CUSTOMIZE_PLAYER_BYTES2 = 1,
    CHAR_CUSTOMIZE_COUNT         = 2
};

/// The number of declined forms a character name has.
enum
{
    CHARACTER_DECLINED_NAME_ROW_CASES = 5
};

/// The callbacks of the asynchronous reads: they run on the world thread with what the reads
/// returned, which they own.
typedef std::function<void(QueryResult*)> CharacterReadCallback;
typedef std::function<void(QueryResult*, SqlQueryHolder*)> CharacterHolderCallback;

/// One `characters` row.
struct CharacterCacheRow
{
    uint32 guid = 0;
    uint32 account = 0;
    std::string name;
    uint8 race = 0;
    uint8 classId = 0;
    uint8 level = 0;
    uint32 zone = 0;
    uint32 map = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
};

/// One `character_skills` row of the login.
struct LoginSkillRow
{
    uint16 skill = 0;
    uint16 value = 0;
    uint16 max = 0;
};

/// One `character_homebind` row of the login.
struct LoginHomeBindRow
{
    uint32 map = 0;
    uint16 zone = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
};

/// One `arena_team_member` row of the login.
struct LoginArenaTeamInfoRow
{
    uint32 arenateamid = 0;
    uint32 playedWeek = 0;
    uint32 playedSeason = 0;
    uint32 wonsSeason = 0;
    uint32 personalRating = 0;
};

/// One instance bind of the login (`character_instance`), with its instance's map, difficulty and
/// reset time.
struct LoginBoundInstanceRow
{
    uint32 id = 0;
    bool permanent = false;
    uint32 map = 0;
    uint8 difficulty = 0;
    uint64 resettime = 0;
};

/// One `character_battleground_data` row of the login.
struct LoginBattlegroundDataRow
{
    uint32 instanceId = 0;
    uint32 team = 0;
    float joinX = 0.0f;
    float joinY = 0.0f;
    float joinZ = 0.0f;
    float joinO = 0.0f;
    uint32 joinMap = 0;
    uint32 taxiStart = 0;
    uint32 taxiEnd = 0;
    uint32 mountSpell = 0;
};

/// One `mail` row of the login.
struct LoginMailRow
{
    uint32 id = 0;
    uint8 messageType = 0;
    uint32 sender = 0;
    uint32 receiver = 0;
    std::string subject;
    std::string body;
    uint64 expireTime = 0;
    uint64 deliverTime = 0;
    uint32 money = 0;
    uint32 cod = 0;
    uint32 checked = 0;
    uint8 stationery = 0;
    int16 mailTemplateId = 0;
    bool hasItems = false;
};

/// One `character_cuf_profiles` row of the login.
struct LoginCufProfileRow
{
    uint8 id = 0;
    std::string name;
    uint16 frameHeight = 0;
    uint16 frameWidth = 0;
    uint8 sortBy = 0;
    uint8 healthText = 0;
    uint32 boolOptions = 0;
    uint8 topPoint = 0;
    uint8 bottomPoint = 0;
    uint8 leftPoint = 0;
    uint16 topOffset = 0;
    uint16 bottomOffset = 0;
    uint16 leftOffset = 0;
};

/// One `character_spell` row of the login.
struct LoginSpellRow
{
    uint32 spell = 0;
    bool active = false;
    bool disabled = false;
};

/// One `character_queststatus_daily` row of the login.
struct LoginDailyQuestStatusRow
{
    uint32 quest = 0;
};

/// One `character_action` row of the login.
struct LoginActionRow
{
    uint8 spec = 0;
    uint8 button = 0;
    uint32 action = 0;
    uint8 type = 0;
};

enum
{
    /// The declined name cases a declined name row holds (genitive to prepositional).
    LOGIN_DECLINED_NAME_ROW_CASES = 5,
    /// The items an equipment set row holds (`item0` to `item18`), one per equipment slot.
    LOGIN_EQUIPMENT_SET_ROW_ITEMS = 19,
    /// The effects an aura row holds base points and a periodic time for.
    LOGIN_AURA_ROW_EFFECTS = 3,
    /// The powers a character row stores (`power1` to `power5`).
    LOGIN_CHARACTER_ROW_POWERS = 5
};

/// The login's declined names (`character_declinedname`), one per case.
struct LoginDeclinedNameRow
{
    std::string name[LOGIN_DECLINED_NAME_ROW_CASES];
};

/// One `character_equipmentsets` row of the login.
struct LoginEquipmentSetRow
{
    uint64 setGuid = 0;
    uint32 setIndex = 0;
    std::string name;
    std::string iconName;
    uint32 ignoreMask = 0;
    uint32 items[LOGIN_EQUIPMENT_SET_ROW_ITEMS] = {};
};

/// One `character_aura` row of the login.
struct LoginAuraRow
{
    uint64 casterGuid = 0;
    uint32 itemGuid = 0;
    uint32 spell = 0;
    uint32 stackCount = 0;
    uint32 remainCharges = 0;
    int32 basePoints[LOGIN_AURA_ROW_EFFECTS] = {};
    uint32 periodicTime[LOGIN_AURA_ROW_EFFECTS] = {};
    int32 maxDuration = 0;
    int32 remainTime = 0;
    uint32 effIndexMask = 0;
};

/// The login's `characters` row: the columns the load reads, the stored powers as one array.
struct LoginCharacterRow
{
    uint32 account = 0;
    std::string name;
    uint8 race = 0;
    uint8 classId = 0;
    uint8 gender = 0;
    uint8 level = 0;
    uint32 xp = 0;
    uint64 money = 0;
    uint32 playerBytes = 0;
    uint32 playerBytes2 = 0;
    uint32 playerFlags = 0;
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    uint32 map = 0;
    float orientation = 0.0f;
    std::optional<std::string> taximask;
    uint32 cinematic = 0;
    uint32 totaltime = 0;
    uint32 leveltime = 0;
    float restBonus = 0.0f;
    uint64 logoutTime = 0;
    int32 isLogoutResting = 0;
    uint32 resettalentsCost = 0;
    uint64 resettalentsTime = 0;
    std::string primaryTrees;
    float transX = 0.0f;
    float transY = 0.0f;
    float transZ = 0.0f;
    float transO = 0.0f;
    uint32 transguid = 0;
    uint32 extraFlags = 0;
    uint32 stableSlots = 0;
    uint32 atLogin = 0;
    uint64 deathExpireTime = 0;
    std::string taxiPath;
    uint32 dungeonDifficulty = 0;
    uint32 totalKills = 0;
    uint16 todayKills = 0;
    uint16 yesterdayKills = 0;
    uint32 chosenTitle = 0;
    int32 watchedFaction = 0;
    uint8 drunk = 0;
    uint32 health = 0;
    uint32 power[LOGIN_CHARACTER_ROW_POWERS] = {};
    uint8 specCount = 0;
    uint8 activeSpec = 0;
    std::optional<std::string> exploredZones;
    std::optional<std::string> knownTitles;
    uint8 actionBars = 0;
    uint8 slot = 0;
    uint32 createdDate = 0;
};

/// The login's `guild_member` row: the guild and the rank.
struct LoginGuildRow
{
    uint32 guildId = 0;
    uint32 rank = 0;
};

/// One completed achievement of the login and its date.
struct LoginAchievementRow
{
    uint32 achievement = 0;
    uint64 date = 0;
};

/// One criteria's progress of the login: its counter and date.
struct LoginCriteriaProgressRow
{
    uint32 criteria = 0;
    uint32 counter = 0;
    uint64 date = 0;
};

/// One `character_pet` row of the login.
struct LoginPetRow
{
    uint32 id = 0;
    uint32 entry = 0;
    uint32 owner = 0;
    uint32 modelId = 0;
    uint32 level = 0;
    uint32 exp = 0;
    uint8 reactState = 0;
    uint32 slot = 0;
    std::string name;
    uint8 renamed = 0;
    uint32 curHealth = 0;
    uint32 curMana = 0;
    std::string abData;
    uint64 saveTime = 0;
    uint32 resetTalentsCost = 0;
    uint64 resetTalentsTime = 0;
    uint32 createdBySpell = 0;
    uint8 petType = 0;
};

/// One `pet_aura` row of the login: the pet it belongs to and the aura.
struct LoginPetAuraRow
{
    uint32 petId = 0;
    uint64 casterGuid = 0;
    uint32 itemGuid = 0;
    uint32 spell = 0;
    uint32 stackCount = 0;
    uint32 remainCharges = 0;
    int32 basePoints[LOGIN_AURA_ROW_EFFECTS] = {};
    uint32 periodicTime[LOGIN_AURA_ROW_EFFECTS] = {};
    int32 maxDuration = 0;
    int32 remainTime = 0;
    uint32 effIndexMask = 0;
};

/// One `pet_spell` row of the login.
struct LoginPetSpellRow
{
    uint32 petId = 0;
    uint32 spell = 0;
    uint8 active = 0;
};

/// One `pet_spell_cooldown` row of the login.
struct LoginPetSpellCooldownRow
{
    uint32 petId = 0;
    uint32 spell = 0;
    uint64 time = 0;
};

/// One `character_pet_declinedname` row of the login: the pet and its name, one per case.
struct LoginPetDeclinedNameRow
{
    uint32 petId = 0;
    std::string name[LOGIN_DECLINED_NAME_ROW_CASES];
};

/// One inventory item of the login: its instance, bag, slot, guid and template.
struct LoginInventoryRow
{
    ItemInstanceRow instance;
    uint32 bag = 0;
    uint8 slot = 0;
    uint32 item = 0;
    uint32 itemTemplate = 0;
};

/// One mailed item of the login: its instance, mail, guid and template.
struct LoginMailedItemRow
{
    ItemInstanceRow instance;
    uint32 mailId = 0;
    uint32 itemGuid = 0;
    uint32 itemTemplate = 0;
};

/// One `item_loot` row of the login: the item it belongs to, and an item id (a currency id when
/// negative, the money when 0) with its amount, random suffix and random property.
struct LoginItemLootRow
{
    uint32 itemGuid = 0;
    int32 itemId = 0;
    uint32 amount = 0;
    uint32 suffix = 0;
    int32 property = 0;
};

/// A character's saved position as the position load reads it, with its taxi path.
struct CharacterPositionRow
{
    float positionX = 0.0f;
    float positionY = 0.0f;
    float positionZ = 0.0f;
    float orientation = 0.0f;
    uint32 map = 0;
    std::string taxiPath;
};

/// The characters' own rows (`characters`, `character_declinedname`) as the character screen, the
/// login, the logout and the account manager use them. The account manager's two reads, the GM
/// commands' reads and the start-up's highest guid and cache rows run on the thread that calls them,
/// synchronously; every other read is asynchronous, and every write is queued
/// on the character database; a write called inside CharacterTransaction joins it.
namespace CharacterRepository
{
    /// Selects the guids of an account's characters.
    QueryResult* QueryAccountCharacters(uint32 accountId);

    /// Selects how many characters an account has on this realm.
    QueryResult* QueryAccountCharacterCount(uint32 accountId);

    /// Selects an offline character's position, map and flight path, synchronously.
    std::optional<CharacterPositionRow> QueryPosition(uint32 guid);

    /// Each sets or clears one of a character's at_login flags.
    void SetAtLoginFlag(uint32 flag, uint32 guid);
    void ClearAtLoginFlag(uint32 flag, uint32 guid);

    /// Sets a character's zone.
    void SetZone(uint32 zone, uint32 guid);

    /// Writes the map a new character's phase data names over the old row.
    void SetPhaseMap(uint32 guid, uint32 map);

    /// Sets a character's gender and appearance bytes, as a customize does.
    void SetAppearance(uint32 gender, uint32 playerBytes, uint32 playerBytes2, uint32 guid);

    /// Asks for the account's characters, ordered by guid, with each one's pet in currentPetSlot
    /// and guild, and its genitive name when declinedNames is set.
    void QueryEnum(uint32 accountId, uint32 currentPetSlot, bool declinedNames, CharacterReadCallback callback);

    /// Asks the login database, in one holder, how many characters the account has across every
    /// realm. A holder the database refuses is deleted and its callback never runs.
    void QueueCreateAccountRead(uint32 accountId, CharacterHolderCallback callback);

    /// Asks, in one holder, for the account's character count, its characters (only the first when
    /// everyCharacter is clear) and the highest pet id. A holder the database refuses is deleted and
    /// its callback never runs.
    void QueueCreateRealmReads(uint32 accountId, bool everyCharacter, CharacterHolderCallback callback);

    /// Stages every read the login of a character needs into a holder, sized to
    /// MAX_PLAYER_LOGIN_QUERY; the declined names' slot stays empty unless declinedNames is set.
    /// Returns false when a read could not be staged.
    bool StageLoginReads(SqlQueryHolder* holder, uint32 guid, bool declinedNames);

    /// Queues the login holder. Returns false when the database refuses it; the holder is then
    /// the caller's to delete.
    bool QueueLoginReads(SqlQueryHolder* holder, CharacterHolderCallback callback);

    /// Marks a character online.
    void SetOnline(uint32 guid);

    /// Marks every character of an account offline.
    void SetAccountOffline(uint32 accountId);

    /// Asks for what a name query answers: a character's guid, name, race, gender and class, and
    /// its five declined names when declinedNames is set.
    void QueryName(uint32 guid, bool declinedNames, CharacterReadCallback callback);

    /// Asks for a character's guid and name when it belongs to the account and has renameFlag
    /// set in at_login.
    void QueryRenameTarget(uint32 guid, uint32 accountId, uint32 renameFlag, CharacterReadCallback callback);

    /// Sets a character's name, the name bound, clears atLoginFlag and deletes its declined names.
    void Rename(uint32 guid, std::string const& name, uint32 atLoginFlag);

    /// Replaces a character's declined names in one transaction, the forms bound.
    void SetDeclinedNames(uint32 guid, std::string const (&names)[CHARACTER_DECLINED_NAME_ROW_CASES]);

    /// Asks, in one holder, for a character's at_login flags and its playerBytes2. A holder the
    /// database refuses is deleted and its callback never runs.
    void QueueCustomizeReads(uint32 guid, CharacterHolderCallback callback);

    /// Sets the character-screen slot of one of the account's characters.
    void SetSlot(uint8 slot, uint32 guid, uint32 accountId);

    /// Selects the highest character guid, at start-up.
    std::optional<uint32> QueryHighestGuid();

    /// Selects every character's cached columns, ordered by guid, at start-up.
    std::vector<CharacterCacheRow> QueryCacheRows();

    /// Selects the accounts of the characters whose name contains the filter, the filter escaped first
    /// through the login database, as the character ban list does.
    QueryResult* QueryAccountsOfCharactersNamed(std::string filter);

    /// Selects an account's characters with their race, class and level, for a GM's character list.
    QueryResult* QueryAccountCharacterList(uint32 accountId);

    /// Marks a character for a customize at its next login.
    void SetCustomizeFlag(uint32 guid);

    /// Marks a character for a rename at its next login.
    void SetRenameFlag(uint32 guid);

    /// Adds at-login flags to a character, as a GM's reset commands do.
    void AddAtLoginFlags(uint32 flags, uint32 guid);

    /// Adds at-login flags to every character that does not have them yet.
    void AddAtLoginFlagsToEveryCharacter(uint32 flags);

    /// Selects a deleted character by its guid.
    QueryResult* QueryDeletedCharacter(uint32 guid);

    /// Selects the deleted characters whose name contains a text, the text written as handed.
    QueryResult* QueryDeletedCharactersNamed(char const* name);

    /// Selects every deleted character.
    QueryResult* QueryDeletedCharacters();

    /// Restores a deleted character under a name and account, the name written as handed.
    void RestoreDeletedCharacter(char const* name, uint32 accountId, uint32 guid);

    /// Sets an offline character's level and clears its experience.
    void SetLevel(uint32 level, uint32 guid);

    /// Selects a character's played time, level, money and account, for a GM's player info.
    QueryResult* QueryPlayerInfo(uint32 guid);

    /// Selects the account of a character by its name, the name already escaped through the login
    /// database, as a ban does.
    QueryResult* QueryAccountOfCharacterNamed(std::string const& name);

    /// Asks for the number of an account's characters on this realm; the callback runs on the world
    /// thread with the result, which it owns.
    void QueryAccountCharacterCountAsync(uint32 accountId, std::function<void(QueryResult*)> callback);

    /// Asks how many characters hold a guid in a range, for the harness's guid block check; answers false
    /// when the database refuses to queue it.
    bool QueryCharacterCountInGuidRange(uint32 first, uint32 last, std::function<void(QueryResult*)> callback);

    /// Marks every character offline, at start-up and shutdown.
    void SetEveryCharacterOffline();

    /// Reads the login holder's skills.
    std::vector<LoginSkillRow> LoginSkills(SqlQueryHolder* holder);

    /// Reads the login holder's group id.
    std::optional<uint32> LoginGroup(SqlQueryHolder* holder);

    /// Reads the login holder's home bind.
    std::optional<LoginHomeBindRow> LoginHomeBind(SqlQueryHolder* holder);

    /// Reads the login holder's arena team info.
    std::vector<LoginArenaTeamInfoRow> LoginArenaTeamInfo(SqlQueryHolder* holder);

    /// Reads the login holder's bound instances.
    std::vector<LoginBoundInstanceRow> LoginBoundInstances(SqlQueryHolder* holder);

    /// Reads the login holder's b g data.
    std::optional<LoginBattlegroundDataRow> LoginBGData(SqlQueryHolder* holder);

    /// Reads the login holder's mails.
    std::vector<LoginMailRow> LoginMails(SqlQueryHolder* holder);

    /// Reads the login holder's c u f profiles.
    std::vector<LoginCufProfileRow> LoginCUFProfiles(SqlQueryHolder* holder);

    /// Reads the login holder's spells.
    std::vector<LoginSpellRow> LoginSpells(SqlQueryHolder* holder);

    /// Reads the login holder's daily quest status.
    std::vector<LoginDailyQuestStatusRow> LoginDailyQuestStatus(SqlQueryHolder* holder);

    /// Reads the login holder's actions.
    std::vector<LoginActionRow> LoginActions(SqlQueryHolder* holder);

    /// Reads the login holder's declined names.
    std::optional<LoginDeclinedNameRow> LoginDeclinedNames(SqlQueryHolder* holder);

    /// Reads the login holder's equipment sets.
    std::vector<LoginEquipmentSetRow> LoginEquipmentSets(SqlQueryHolder* holder);

    /// Reads the login holder's auras.
    std::vector<LoginAuraRow> LoginAuras(SqlQueryHolder* holder);

    /// Reads the login holder's inventory items.
    std::vector<LoginInventoryRow> LoginInventory(SqlQueryHolder* holder);

    /// Reads the login holder's mailed items.
    std::vector<LoginMailedItemRow> LoginMailedItems(SqlQueryHolder* holder);

    /// Reads the login holder's item loot.
    std::vector<LoginItemLootRow> LoginItemLoot(SqlQueryHolder* holder);

    /// Reads the login holder's character row, none when the character does not exist.
    std::optional<LoginCharacterRow> LoginCharacter(SqlQueryHolder* holder);

    /// Reads the login holder's guild membership, none when the character is in no guild.
    std::optional<LoginGuildRow> LoginGuild(SqlQueryHolder* holder);

    /// Reads the login holder's completed achievements.
    std::vector<LoginAchievementRow> LoginAchievements(SqlQueryHolder* holder);

    /// Reads the login holder's criteria progress.
    std::vector<LoginCriteriaProgressRow> LoginCriteriaProgress(SqlQueryHolder* holder);

    /// Reads the login holder's pets, ordered by id.
    std::vector<LoginPetRow> LoginPets(SqlQueryHolder* holder);

    /// Reads the login holder's pet auras, ordered by pet.
    std::vector<LoginPetAuraRow> LoginPetAuras(SqlQueryHolder* holder);

    /// Reads the login holder's pet spells, ordered by pet and spell.
    std::vector<LoginPetSpellRow> LoginPetSpells(SqlQueryHolder* holder);

    /// Reads the login holder's pet spell cooldowns, ordered by pet and spell.
    std::vector<LoginPetSpellCooldownRow> LoginPetSpellCooldowns(SqlQueryHolder* holder);

    /// Reads the login holder's pet declined names, ordered by pet.
    std::vector<LoginPetDeclinedNameRow> LoginPetDeclinedNames(SqlQueryHolder* holder);
}

#endif
