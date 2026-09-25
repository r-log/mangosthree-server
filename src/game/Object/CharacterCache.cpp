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

#include "CharacterCache.h"

#include "ArenaTeam.h"
#include "Database/DatabaseEnv.h"
#include "Log.h"
#include "Player.h"
#include "ProgressBar.h"
#include "Util.h"

static_assert(CHARACTER_CACHE_ARENA_SLOTS == MAX_ARENA_SLOT,
              "the cache's arena slot count has to be the game's");

namespace
{
    /**
     * @brief utf8_general_ci's canonical form, one code point per entry.
     *
     * The name index has to answer what `SELECT ... WHERE name = '...'` answered, and the
     * `characters`.`name` column is **utf8_general_ci** -- which is not merely
     * case-insensitive. Under it 'Bjoern with a diaeresis' equals 'Bjorn' and the sharp s
     * equals a single 's', while several letters that look like accented Latin are NOT
     * equal to their base letter. Folding case alone (what this used to do) would have let
     * a second "Bjorn" be created beside an existing one, and would have made an unaccented
     * GM lookup miss the accented character -- while the rename duplicate check, which is
     * still a SQL comparison (CharacterHandlerCustomize.cpp), went on disagreeing.
     *
     * So the table is not written by hand or by an accent-stripping rule: it is GENERATED
     * from the collation itself, by asking the live server for the weight of every code
     * point in EVERY RANGE ObjectMgr::CheckPlayerName admits -- U+0020..U+024F (all of
     * Latin) and U+1E9E, U+0400..U+045F (Cyrillic), and the eight East Asian ranges
     * Util.h's isEastAsianCharacter lists, U+1100..U+11F9, U+3041..U+30FF, U+3131..U+318E,
     * U+31F0..U+31FF, U+3400..U+4DB5, U+4E00..U+9FC3, U+AC00..U+D7A3 and U+FF01..U+FFEE:
     *
     *     SELECT HEX(WEIGHT_STRING(CONVERT(UNHEX(<utf8 hex>) USING utf8)
     *                              COLLATE utf8_general_ci));
     *
     * Two characters are equal under the collation exactly when those weights are equal
     * (utf8_general_ci has one weight per character and no expansions), so each code point
     * folds to the LOWEST code point sharing its weight. MariaDB 10.4.32, `character3`.
     * What it answered, for the cases worth naming:
     *
     *     'o with diaeresis' = 'o'   1        ae ligature = "ae"   0
     *     'e with acute'     = 'e'   1        'o with stroke' = 'o'  0
     *     'a with ring'      = 'a'   1        eth = 'd'              0
     *     'n with tilde'     = 'n'   1        thorn = 't'            0
     *     sharp s = 's'              1        'l with stroke' = 'l'  0
     *     sharp s = "ss"             0        'd with stroke' = 'd'  0
     *     CYRILLIC IO = CYRILLIC E   1 (both cases, and across case)
     *
     * Seven of the eight East Asian ranges came back as pure identity -- every Hangul,
     * kana and CJK character weighs as itself -- so the table carries nothing for them.
     * The eighth did not: in Halfwidth and Fullwidth Forms the 26 fullwidth small letters
     * fold to the fullwidth capitals, and those 26 entries are in the table. They are the
     * reason the fold cannot be described as "Latin and Cyrillic only".
     *
     * A code point outside the probed ranges is left alone. That is a decision, not a law:
     * the collation folds plenty that is not in this table (Greek case, MICRO SIGN), and
     * the table is silent about all of it because ObjectMgr::CheckPlayerName does not admit
     * it as a character name. If the name validation ever widens, the generator has to be
     * re-run over the new range.
     *
     * Sorted by `from`, so the lookup below is a binary search. A CJK character now falls
     * inside the table's span and searches it; it misses, which is the right answer, at the
     * cost of a handful of comparisons.
     */
    struct CollationFold
    {
        uint16 from;
        uint16 to;
    };

    const CollationFold kGeneralCiFold[] =
    {
    { 0x00C0, 0x0041 }, { 0x00C1, 0x0041 }, { 0x00C2, 0x0041 }, { 0x00C3, 0x0041 }, { 0x00C4, 0x0041 }, { 0x00C5, 0x0041 },
    { 0x00C7, 0x0043 }, { 0x00C8, 0x0045 }, { 0x00C9, 0x0045 }, { 0x00CA, 0x0045 }, { 0x00CB, 0x0045 }, { 0x00CC, 0x0049 },
    { 0x00CD, 0x0049 }, { 0x00CE, 0x0049 }, { 0x00CF, 0x0049 }, { 0x00D1, 0x004E }, { 0x00D2, 0x004F }, { 0x00D3, 0x004F },
    { 0x00D4, 0x004F }, { 0x00D5, 0x004F }, { 0x00D6, 0x004F }, { 0x00D9, 0x0055 }, { 0x00DA, 0x0055 }, { 0x00DB, 0x0055 },
    { 0x00DC, 0x0055 }, { 0x00DD, 0x0059 }, { 0x00DF, 0x0053 }, { 0x00E0, 0x0041 }, { 0x00E1, 0x0041 }, { 0x00E2, 0x0041 },
    { 0x00E3, 0x0041 }, { 0x00E4, 0x0041 }, { 0x00E5, 0x0041 }, { 0x00E6, 0x00C6 }, { 0x00E7, 0x0043 }, { 0x00E8, 0x0045 },
    { 0x00E9, 0x0045 }, { 0x00EA, 0x0045 }, { 0x00EB, 0x0045 }, { 0x00EC, 0x0049 }, { 0x00ED, 0x0049 }, { 0x00EE, 0x0049 },
    { 0x00EF, 0x0049 }, { 0x00F0, 0x00D0 }, { 0x00F1, 0x004E }, { 0x00F2, 0x004F }, { 0x00F3, 0x004F }, { 0x00F4, 0x004F },
    { 0x00F5, 0x004F }, { 0x00F6, 0x004F }, { 0x00F8, 0x00D8 }, { 0x00F9, 0x0055 }, { 0x00FA, 0x0055 }, { 0x00FB, 0x0055 },
    { 0x00FC, 0x0055 }, { 0x00FD, 0x0059 }, { 0x00FE, 0x00DE }, { 0x00FF, 0x0059 }, { 0x0100, 0x0041 }, { 0x0101, 0x0041 },
    { 0x0102, 0x0041 }, { 0x0103, 0x0041 }, { 0x0104, 0x0041 }, { 0x0105, 0x0041 }, { 0x0106, 0x0043 }, { 0x0107, 0x0043 },
    { 0x0108, 0x0043 }, { 0x0109, 0x0043 }, { 0x010A, 0x0043 }, { 0x010B, 0x0043 }, { 0x010C, 0x0043 }, { 0x010D, 0x0043 },
    { 0x010E, 0x0044 }, { 0x010F, 0x0044 }, { 0x0111, 0x0110 }, { 0x0112, 0x0045 }, { 0x0113, 0x0045 }, { 0x0114, 0x0045 },
    { 0x0115, 0x0045 }, { 0x0116, 0x0045 }, { 0x0117, 0x0045 }, { 0x0118, 0x0045 }, { 0x0119, 0x0045 }, { 0x011A, 0x0045 },
    { 0x011B, 0x0045 }, { 0x011C, 0x0047 }, { 0x011D, 0x0047 }, { 0x011E, 0x0047 }, { 0x011F, 0x0047 }, { 0x0120, 0x0047 },
    { 0x0121, 0x0047 }, { 0x0122, 0x0047 }, { 0x0123, 0x0047 }, { 0x0124, 0x0048 }, { 0x0125, 0x0048 }, { 0x0127, 0x0126 },
    { 0x0128, 0x0049 }, { 0x0129, 0x0049 }, { 0x012A, 0x0049 }, { 0x012B, 0x0049 }, { 0x012C, 0x0049 }, { 0x012D, 0x0049 },
    { 0x012E, 0x0049 }, { 0x012F, 0x0049 }, { 0x0130, 0x0049 }, { 0x0131, 0x0049 }, { 0x0133, 0x0132 }, { 0x0134, 0x004A },
    { 0x0135, 0x004A }, { 0x0136, 0x004B }, { 0x0137, 0x004B }, { 0x0139, 0x004C }, { 0x013A, 0x004C }, { 0x013B, 0x004C },
    { 0x013C, 0x004C }, { 0x013D, 0x004C }, { 0x013E, 0x004C }, { 0x0140, 0x013F }, { 0x0142, 0x0141 }, { 0x0143, 0x004E },
    { 0x0144, 0x004E }, { 0x0145, 0x004E }, { 0x0146, 0x004E }, { 0x0147, 0x004E }, { 0x0148, 0x004E }, { 0x014B, 0x014A },
    { 0x014C, 0x004F }, { 0x014D, 0x004F }, { 0x014E, 0x004F }, { 0x014F, 0x004F }, { 0x0150, 0x004F }, { 0x0151, 0x004F },
    { 0x0153, 0x0152 }, { 0x0154, 0x0052 }, { 0x0155, 0x0052 }, { 0x0156, 0x0052 }, { 0x0157, 0x0052 }, { 0x0158, 0x0052 },
    { 0x0159, 0x0052 }, { 0x015A, 0x0053 }, { 0x015B, 0x0053 }, { 0x015C, 0x0053 }, { 0x015D, 0x0053 }, { 0x015E, 0x0053 },
    { 0x015F, 0x0053 }, { 0x0160, 0x0053 }, { 0x0161, 0x0053 }, { 0x0162, 0x0054 }, { 0x0163, 0x0054 }, { 0x0164, 0x0054 },
    { 0x0165, 0x0054 }, { 0x0167, 0x0166 }, { 0x0168, 0x0055 }, { 0x0169, 0x0055 }, { 0x016A, 0x0055 }, { 0x016B, 0x0055 },
    { 0x016C, 0x0055 }, { 0x016D, 0x0055 }, { 0x016E, 0x0055 }, { 0x016F, 0x0055 }, { 0x0170, 0x0055 }, { 0x0171, 0x0055 },
    { 0x0172, 0x0055 }, { 0x0173, 0x0055 }, { 0x0174, 0x0057 }, { 0x0175, 0x0057 }, { 0x0176, 0x0059 }, { 0x0177, 0x0059 },
    { 0x0178, 0x0059 }, { 0x0179, 0x005A }, { 0x017A, 0x005A }, { 0x017B, 0x005A }, { 0x017C, 0x005A }, { 0x017D, 0x005A },
    { 0x017E, 0x005A }, { 0x017F, 0x0053 }, { 0x0183, 0x0182 }, { 0x0185, 0x0184 }, { 0x0188, 0x0187 }, { 0x018C, 0x018B },
    { 0x0192, 0x0191 }, { 0x0199, 0x0198 }, { 0x01A0, 0x004F }, { 0x01A1, 0x004F }, { 0x01A3, 0x01A2 }, { 0x01A5, 0x01A4 },
    { 0x01A8, 0x01A7 }, { 0x01AD, 0x01AC }, { 0x01AF, 0x0055 }, { 0x01B0, 0x0055 }, { 0x01B4, 0x01B3 }, { 0x01B6, 0x01B5 },
    { 0x01B9, 0x01B8 }, { 0x01BD, 0x01BC }, { 0x01C5, 0x01C4 }, { 0x01C6, 0x01C4 }, { 0x01C8, 0x01C7 }, { 0x01C9, 0x01C7 },
    { 0x01CB, 0x01CA }, { 0x01CC, 0x01CA }, { 0x01CD, 0x0041 }, { 0x01CE, 0x0041 }, { 0x01CF, 0x0049 }, { 0x01D0, 0x0049 },
    { 0x01D1, 0x004F }, { 0x01D2, 0x004F }, { 0x01D3, 0x0055 }, { 0x01D4, 0x0055 }, { 0x01D5, 0x0055 }, { 0x01D6, 0x0055 },
    { 0x01D7, 0x0055 }, { 0x01D8, 0x0055 }, { 0x01D9, 0x0055 }, { 0x01DA, 0x0055 }, { 0x01DB, 0x0055 }, { 0x01DC, 0x0055 },
    { 0x01DD, 0x018E }, { 0x01DE, 0x0041 }, { 0x01DF, 0x0041 }, { 0x01E0, 0x0041 }, { 0x01E1, 0x0041 }, { 0x01E2, 0x00C6 },
    { 0x01E3, 0x00C6 }, { 0x01E5, 0x01E4 }, { 0x01E6, 0x0047 }, { 0x01E7, 0x0047 }, { 0x01E8, 0x004B }, { 0x01E9, 0x004B },
    { 0x01EA, 0x004F }, { 0x01EB, 0x004F }, { 0x01EC, 0x004F }, { 0x01ED, 0x004F }, { 0x01EE, 0x01B7 }, { 0x01EF, 0x01B7 },
    { 0x01F0, 0x004A }, { 0x01F2, 0x01F1 }, { 0x01F3, 0x01F1 }, { 0x01F4, 0x0047 }, { 0x01F5, 0x0047 }, { 0x01F6, 0x0195 },
    { 0x01F7, 0x01BF }, { 0x01F8, 0x004E }, { 0x01F9, 0x004E }, { 0x01FA, 0x0041 }, { 0x01FB, 0x0041 }, { 0x01FC, 0x00C6 },
    { 0x01FD, 0x00C6 }, { 0x01FE, 0x00D8 }, { 0x01FF, 0x00D8 }, { 0x0200, 0x0041 }, { 0x0201, 0x0041 }, { 0x0202, 0x0041 },
    { 0x0203, 0x0041 }, { 0x0204, 0x0045 }, { 0x0205, 0x0045 }, { 0x0206, 0x0045 }, { 0x0207, 0x0045 }, { 0x0208, 0x0049 },
    { 0x0209, 0x0049 }, { 0x020A, 0x0049 }, { 0x020B, 0x0049 }, { 0x020C, 0x004F }, { 0x020D, 0x004F }, { 0x020E, 0x004F },
    { 0x020F, 0x004F }, { 0x0210, 0x0052 }, { 0x0211, 0x0052 }, { 0x0212, 0x0052 }, { 0x0213, 0x0052 }, { 0x0214, 0x0055 },
    { 0x0215, 0x0055 }, { 0x0216, 0x0055 }, { 0x0217, 0x0055 }, { 0x0218, 0x0053 }, { 0x0219, 0x0053 }, { 0x021A, 0x0054 },
    { 0x021B, 0x0054 }, { 0x021D, 0x021C }, { 0x021E, 0x0048 }, { 0x021F, 0x0048 }, { 0x0223, 0x0222 }, { 0x0225, 0x0224 },
    { 0x0226, 0x0041 }, { 0x0227, 0x0041 }, { 0x0228, 0x0045 }, { 0x0229, 0x0045 }, { 0x022A, 0x004F }, { 0x022B, 0x004F },
    { 0x022C, 0x004F }, { 0x022D, 0x004F }, { 0x022E, 0x004F }, { 0x022F, 0x004F }, { 0x0230, 0x004F }, { 0x0231, 0x004F },
    { 0x0232, 0x0059 }, { 0x0233, 0x0059 }, { 0x0401, 0x0400 }, { 0x0407, 0x0406 }, { 0x0413, 0x0403 }, { 0x0415, 0x0400 },
    { 0x0418, 0x040D }, { 0x041A, 0x040C }, { 0x0423, 0x040E }, { 0x0430, 0x0410 }, { 0x0431, 0x0411 }, { 0x0432, 0x0412 },
    { 0x0433, 0x0403 }, { 0x0434, 0x0414 }, { 0x0435, 0x0400 }, { 0x0436, 0x0416 }, { 0x0437, 0x0417 }, { 0x0438, 0x040D },
    { 0x0439, 0x0419 }, { 0x043A, 0x040C }, { 0x043B, 0x041B }, { 0x043C, 0x041C }, { 0x043D, 0x041D }, { 0x043E, 0x041E },
    { 0x043F, 0x041F }, { 0x0440, 0x0420 }, { 0x0441, 0x0421 }, { 0x0442, 0x0422 }, { 0x0443, 0x040E }, { 0x0444, 0x0424 },
    { 0x0445, 0x0425 }, { 0x0446, 0x0426 }, { 0x0447, 0x0427 }, { 0x0448, 0x0428 }, { 0x0449, 0x0429 }, { 0x044A, 0x042A },
    { 0x044B, 0x042B }, { 0x044C, 0x042C }, { 0x044D, 0x042D }, { 0x044E, 0x042E }, { 0x044F, 0x042F }, { 0x0450, 0x0400 },
    { 0x0451, 0x0400 }, { 0x0452, 0x0402 }, { 0x0453, 0x0403 }, { 0x0454, 0x0404 }, { 0x0455, 0x0405 }, { 0x0456, 0x0406 },
    { 0x0457, 0x0406 }, { 0x0458, 0x0408 }, { 0x0459, 0x0409 }, { 0x045A, 0x040A }, { 0x045B, 0x040B }, { 0x045C, 0x040C },
    { 0x045D, 0x040D }, { 0x045E, 0x040E }, { 0x045F, 0x040F },
    // Halfwidth and Fullwidth Forms. The probe over the East Asian ranges came back
    // identity everywhere except these 26: the fullwidth small letters fold to the
    // fullwidth capitals. They do NOT fold to ASCII -- U+FF21 weighs as itself, not as 'A'.
    { 0xFF41, 0xFF21 }, { 0xFF42, 0xFF22 }, { 0xFF43, 0xFF23 }, { 0xFF44, 0xFF24 }, { 0xFF45, 0xFF25 }, { 0xFF46, 0xFF26 },
    { 0xFF47, 0xFF27 }, { 0xFF48, 0xFF28 }, { 0xFF49, 0xFF29 }, { 0xFF4A, 0xFF2A }, { 0xFF4B, 0xFF2B }, { 0xFF4C, 0xFF2C },
    { 0xFF4D, 0xFF2D }, { 0xFF4E, 0xFF2E }, { 0xFF4F, 0xFF2F }, { 0xFF50, 0xFF30 }, { 0xFF51, 0xFF31 }, { 0xFF52, 0xFF32 },
    { 0xFF53, 0xFF33 }, { 0xFF54, 0xFF34 }, { 0xFF55, 0xFF35 }, { 0xFF56, 0xFF36 }, { 0xFF57, 0xFF37 }, { 0xFF58, 0xFF38 },
    { 0xFF59, 0xFF39 }, { 0xFF5A, 0xFF3A },
    };

    const uint16 kFoldFirst = kGeneralCiFold[0].from;
    const uint16 kFoldLast  = kGeneralCiFold[sizeof(kGeneralCiFold) / sizeof(kGeneralCiFold[0]) - 1].from;

    /// One character in utf8_general_ci's canonical form.
    wchar_t FoldChar(wchar_t wide)
    {
        if (wide >= L'a' && wide <= L'z')
        {
            return wchar_t(wide - 32);                      // the table's own rule, done cheaply
        }

        if (wide < wchar_t(kFoldFirst) || wide > wchar_t(kFoldLast))
        {
            return wide;
        }

        const uint16 needle = uint16(wide);
        size_t lo = 0;
        size_t hi = sizeof(kGeneralCiFold) / sizeof(kGeneralCiFold[0]);
        while (lo < hi)
        {
            const size_t mid = lo + (hi - lo) / 2;
            if (kGeneralCiFold[mid].from < needle)
            {
                lo = mid + 1;
            }
            else if (kGeneralCiFold[mid].from > needle)
            {
                hi = mid;
            }
            else
            {
                return wchar_t(kGeneralCiFold[mid].to);
            }
        }

        return wide;
    }
}

std::string CharacterCache::FoldName(std::string const& name)
{
    if (name.empty())
    {
        return name;
    }

    std::wstring wide;
    if (!Utf8toWStr(name, wide))
    {
        return name;                                        // not valid UTF-8: key it as it came
    }

    for (size_t i = 0; i < wide.size(); ++i)
    {
        wide[i] = FoldChar(wide[i]);
    }

    std::string folded;
    if (!WStrToUtf8(wide, folded))
    {
        return name;
    }

    return folded;
}

void CharacterCache::LoadFromDB()
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    m_entries.clear();
    m_byName.clear();
    m_zoneless.clear();

    uint32 count = 0;

    // ORDER BY `guid` is load-bearing, not tidiness: `idx_name` is NOT unique, so two rows
    // may share a name, and the SELECT this index replaces answered the lowest guid of
    // them (it walked `idx_name`, whose entries for one name are ordered by the clustered
    // key). The name index below keeps the FIRST row it sees for a name, so the rows have
    // to arrive in guid order for "first" to mean what the old answer meant. `guid` is the
    // primary key, so this is an index scan, not a sort.
    //                                                     0       1         2       3       4        5        6       7      8             9             10
    QueryResult* result = CharacterDatabase.Query("SELECT `guid`, `account`, `name`, `race`, `class`, `level`, `zone`, `map`, `position_x`, `position_y`, `position_z` FROM `characters` ORDER BY `guid`");
    if (result)
    {
        BarGoLink bar(result->GetRowCount());
        do
        {
            bar.step();

            Field* fields = result->Fetch();

            std::shared_ptr<CharacterCacheEntry> entry = std::make_shared<CharacterCacheEntry>();
            entry->guid        = ObjectGuid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            entry->accountId   = fields[1].GetUInt32();
            entry->name        = fields[2].GetCppString();
            entry->race        = fields[3].GetUInt8();
            entry->playerClass = fields[4].GetUInt8();
            entry->level       = fields[5].GetUInt8();
            entry->zoneId      = fields[6].GetUInt32();

            if (!entry->zoneId)
            {
                // The only thing a position was ever wanted for: Player::GetZoneIdFromDB
                // recomputes the zone from it when the stored one is zero. Kept for those
                // rows alone, and dropped as soon as one is repaired.
                Position where;
                where.mapId = fields[7].GetUInt32();
                where.x     = fields[8].GetFloat();
                where.y     = fields[9].GetFloat();
                where.z     = fields[10].GetFloat();
                m_zoneless[entry->guid] = where;
            }

            std::string const folded = FoldName(entry->name);
            if (!folded.empty())
            {
                // First wins, and the rows arrive in guid order: a name two rows share
                // answers the lower guid, as the old SELECT did. Not an assignment --
                // that would have answered the LAST row scanned instead.
                m_byName.emplace(folded, entry->guid);
            }

            m_entries[entry->guid] = entry;
            ++count;
        }
        while (result->NextRow());

        delete result;
    }
    else
    {
        BarGoLink bar(1);
        bar.step();
    }

    uint32 guildMembers = 0;
    //                                                0       1          2
    result = CharacterDatabase.Query("SELECT `guid`, `guildid`, `rank` FROM `guild_member`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            const ObjectGuid guid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            EntryMap::const_iterator itr = m_entries.find(guid);
            if (itr == m_entries.end())
            {
                continue;                                   // a guild row with no character
            }

            itr->second->guildId   = fields[1].GetUInt32();
            itr->second->guildRank = fields[2].GetUInt32();
            ++guildMembers;
        }
        while (result->NextRow());

        delete result;
    }

    uint32 arenaMembers = 0;
    //                                                                              0                   1                  2
    result = CharacterDatabase.Query("SELECT `arena_team_member`.`guid`, `arena_team_member`.`arenateamid`, `arena_team`.`type` "
                                     "FROM `arena_team_member` JOIN `arena_team` ON `arena_team_member`.`arenateamid` = `arena_team`.`arenateamid`");
    if (result)
    {
        do
        {
            Field* fields = result->Fetch();

            const ObjectGuid guid(HIGHGUID_PLAYER, fields[0].GetUInt32());
            EntryMap::const_iterator itr = m_entries.find(guid);
            if (itr == m_entries.end())
            {
                continue;                                   // an arena row with no character
            }

            const uint8 slot = ArenaTeam::GetSlotByType(ArenaType(fields[2].GetUInt32()));
            if (slot >= CHARACTER_CACHE_ARENA_SLOTS)
            {
                continue;                                   // GetSlotByType has already logged it
            }

            itr->second->arenaTeamId[slot] = fields[1].GetUInt32();
            ++arenaMembers;
        }
        while (result->NextRow());

        delete result;
    }

    sLog.outString(">> Loaded %u characters into the character cache (%u guild, %u arena memberships)",
                   count, guildMembers, arenaMembers);
    sLog.outString();
}

CharacterCacheRef CharacterCache::GetByGuid(ObjectGuid guid) const
{
    std::shared_lock<std::shared_mutex> guard(m_lock);

    EntryMap::const_iterator itr = m_entries.find(guid);
    return itr != m_entries.end() ? CharacterCacheRef(itr->second) : CharacterCacheRef();
}

CharacterCacheRef CharacterCache::GetByName(std::string const& name) const
{
    const std::string folded = FoldName(name);
    if (folded.empty())
    {
        return CharacterCacheRef();
    }

    std::shared_lock<std::shared_mutex> guard(m_lock);

    NameMap::const_iterator named = m_byName.find(folded);
    if (named == m_byName.end())
    {
        return CharacterCacheRef();
    }

    EntryMap::const_iterator itr = m_entries.find(named->second);
    return itr != m_entries.end() ? CharacterCacheRef(itr->second) : CharacterCacheRef();
}

Team CharacterCache::GetTeam(ObjectGuid guid) const
{
    if (CharacterCacheRef entry = GetByGuid(guid))
    {
        return Player::TeamForRace(entry->race);
    }

    return TEAM_NONE;
}

uint32 CharacterCache::GetAccountId(ObjectGuid guid) const
{
    if (CharacterCacheRef entry = GetByGuid(guid))
    {
        return entry->accountId;
    }

    return 0;
}

size_t CharacterCache::Size() const
{
    std::shared_lock<std::shared_mutex> guard(m_lock);
    return m_entries.size();
}

void CharacterCache::ReindexNameLocked(std::string const& oldName, std::string const& newName,
                                       ObjectGuid guid)
{
    const std::string oldKey = FoldName(oldName);
    const std::string newKey = FoldName(newName);

    if (oldKey == newKey)
    {
        return;
    }

    if (!oldKey.empty())
    {
        NameMap::const_iterator itr = m_byName.find(oldKey);
        if (itr != m_byName.end() && itr->second == guid)
        {
            m_byName.erase(itr);
        }
    }

    if (!newKey.empty())
    {
        m_byName[newKey] = guid;
    }
}

template <typename F>
void CharacterCache::Mutate(ObjectGuid guid, F&& change)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    EntryMap::iterator itr = m_entries.find(guid);
    if (itr == m_entries.end())
    {
        return;
    }

    // Copy-on-write: whoever is reading the old entry right now keeps reading a whole,
    // consistent one; the next reader gets this copy.
    std::shared_ptr<CharacterCacheEntry> updated =
        std::make_shared<CharacterCacheEntry>(*itr->second);
    change(*updated);

    if (updated->name != itr->second->name)
    {
        ReindexNameLocked(itr->second->name, updated->name, guid);
    }

    itr->second = updated;
}

void CharacterCache::Add(CharacterCacheEntry const& entry)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    EntryMap::iterator itr = m_entries.find(entry.guid);
    if (itr != m_entries.end())
    {
        ReindexNameLocked(itr->second->name, entry.name, entry.guid);
    }
    else
    {
        const std::string key = FoldName(entry.name);
        if (!key.empty())
        {
            // Insert-if-absent, for the same reason as the load: `idx_name` is not unique,
            // so a character may be created or dump-loaded under a name another character
            // already holds (`.pdump load` does exactly that, and flags the new one to be
            // renamed at login). The new character always has the higher guid, so the
            // holder of the name keeps answering -- which is the row the old SELECT found.
            m_byName.emplace(key, entry.guid);
        }
    }

    m_entries[entry.guid] = std::make_shared<CharacterCacheEntry>(entry);
    m_zoneless.erase(entry.guid);
}

void CharacterCache::Remove(ObjectGuid guid)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    EntryMap::const_iterator itr = m_entries.find(guid);
    if (itr == m_entries.end())
    {
        return;
    }

    const std::string key = FoldName(itr->second->name);
    if (!key.empty())
    {
        NameMap::const_iterator named = m_byName.find(key);
        if (named != m_byName.end() && named->second == guid)
        {
            // The guard matters when two rows share a name: dropping the one that does not
            // hold the key leaves the holder answering, as the database would. Dropping the
            // holder does free the name, and the other row stops being findable by it until
            // a restart -- the one place this index is coarser than `idx_name`, and it needs
            // duplicate names to reach at all.
            m_byName.erase(named);
        }
    }

    m_entries.erase(itr);
    m_zoneless.erase(guid);
}

void CharacterCache::UpdateName(ObjectGuid guid, std::string const& name)
{
    Mutate(guid, [&name](CharacterCacheEntry& entry) { entry.name = name; });
}

void CharacterCache::UpdateAccount(ObjectGuid guid, uint32 accountId)
{
    Mutate(guid, [accountId](CharacterCacheEntry& entry) { entry.accountId = accountId; });
}

void CharacterCache::UpdateLevel(ObjectGuid guid, uint8 level)
{
    Mutate(guid, [level](CharacterCacheEntry& entry) { entry.level = level; });
}

void CharacterCache::UpdateZone(ObjectGuid guid, uint32 zoneId)
{
    Mutate(guid, [zoneId](CharacterCacheEntry& entry) { entry.zoneId = zoneId; });

    if (zoneId)
    {
        ForgetZonelessPosition(guid);
    }
}

void CharacterCache::UpdateGuild(ObjectGuid guid, uint32 guildId, uint32 rank /*= 0*/)
{
    Mutate(guid, [guildId, rank](CharacterCacheEntry& entry)
    {
        entry.guildId   = guildId;
        entry.guildRank = guildId ? rank : 0;
    });
}

void CharacterCache::UpdateGuildRank(ObjectGuid guid, uint32 rank)
{
    Mutate(guid, [rank](CharacterCacheEntry& entry) { entry.guildRank = rank; });
}

void CharacterCache::ClearGuild(uint32 guildId)
{
    if (!guildId)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> guard(m_lock);

    for (EntryMap::iterator itr = m_entries.begin(); itr != m_entries.end(); ++itr)
    {
        if (itr->second->guildId != guildId)
        {
            continue;
        }

        std::shared_ptr<CharacterCacheEntry> updated =
            std::make_shared<CharacterCacheEntry>(*itr->second);
        updated->guildId   = 0;
        updated->guildRank = 0;
        itr->second = updated;
    }
}

void CharacterCache::UpdateArenaTeam(ObjectGuid guid, uint8 slot, uint32 arenaTeamId)
{
    if (slot >= CHARACTER_CACHE_ARENA_SLOTS)
    {
        return;
    }

    Mutate(guid, [slot, arenaTeamId](CharacterCacheEntry& entry)
    {
        entry.arenaTeamId[slot] = arenaTeamId;
    });
}

void CharacterCache::ClearArenaTeam(uint32 arenaTeamId)
{
    if (!arenaTeamId)
    {
        return;
    }

    std::unique_lock<std::shared_mutex> guard(m_lock);

    for (EntryMap::iterator itr = m_entries.begin(); itr != m_entries.end(); ++itr)
    {
        bool member = false;
        for (uint8 slot = 0; slot < CHARACTER_CACHE_ARENA_SLOTS; ++slot)
        {
            if (itr->second->arenaTeamId[slot] == arenaTeamId)
            {
                member = true;
            }
        }

        if (!member)
        {
            continue;
        }

        std::shared_ptr<CharacterCacheEntry> updated =
            std::make_shared<CharacterCacheEntry>(*itr->second);
        for (uint8 slot = 0; slot < CHARACTER_CACHE_ARENA_SLOTS; ++slot)
        {
            if (updated->arenaTeamId[slot] == arenaTeamId)
            {
                updated->arenaTeamId[slot] = 0;
            }
        }
        itr->second = updated;
    }
}

bool CharacterCache::GetPositionForZonelessCharacter(ObjectGuid guid, uint32& mapId,
                                                     float& x, float& y, float& z) const
{
    std::shared_lock<std::shared_mutex> guard(m_lock);

    PositionMap::const_iterator itr = m_zoneless.find(guid);
    if (itr == m_zoneless.end())
    {
        return false;
    }

    mapId = itr->second.mapId;
    x     = itr->second.x;
    y     = itr->second.y;
    z     = itr->second.z;
    return true;
}

void CharacterCache::ForgetZonelessPosition(ObjectGuid guid)
{
    std::unique_lock<std::shared_mutex> guard(m_lock);
    m_zoneless.erase(guid);
}

void CharacterCache::Clear()
{
    std::unique_lock<std::shared_mutex> guard(m_lock);

    m_entries.clear();
    m_byName.clear();
    m_zoneless.clear();
}
