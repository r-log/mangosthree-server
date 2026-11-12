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

// The result codes the server sends on the character screen, pinned to the 4.3.4 client's own
// table (ResponseCodes, 0 to 105). The client indexes its message tokens by code
// (s_errorTokens, ClientServices.cpp, 0x00CA0970/0x140B3CCA0), so a code one off shows
// another refusal's text.
//
// Literals on purpose: the enum is what is under test.

#include "SharedDefines.h"

static_assert(RESPONSE_SUCCESS                                       == 0);
static_assert(RESPONSE_FAILURE                                       == 1);
static_assert(RESPONSE_CANCELLED                                     == 2);
static_assert(RESPONSE_DISCONNECTED                                  == 3);
static_assert(RESPONSE_FAILED_TO_CONNECT                             == 4);
static_assert(RESPONSE_CONNECTED                                     == 5);
static_assert(RESPONSE_VERSION_MISMATCH                              == 6);
static_assert(CSTATUS_CONNECTING                                     == 7);
static_assert(CSTATUS_NEGOTIATING_SECURITY                           == 8);
static_assert(CSTATUS_NEGOTIATION_COMPLETE                           == 9);
static_assert(CSTATUS_NEGOTIATION_FAILED                             == 10);
static_assert(CSTATUS_AUTHENTICATING                                 == 11);
static_assert(AUTH_OK                                                == 12);
static_assert(AUTH_FAILED                                            == 13);
static_assert(AUTH_REJECT                                            == 14);
static_assert(AUTH_BAD_SERVER_PROOF                                  == 15);
static_assert(AUTH_UNAVAILABLE                                       == 16);
static_assert(AUTH_SYSTEM_ERROR                                      == 17);
static_assert(AUTH_BILLING_ERROR                                     == 18);
static_assert(AUTH_BILLING_EXPIRED                                   == 19);
static_assert(AUTH_VERSION_MISMATCH                                  == 20);
static_assert(AUTH_UNKNOWN_ACCOUNT                                   == 21);
static_assert(AUTH_INCORRECT_PASSWORD                                == 22);
static_assert(AUTH_SESSION_EXPIRED                                   == 23);
static_assert(AUTH_SERVER_SHUTTING_DOWN                              == 24);
static_assert(AUTH_ALREADY_LOGGING_IN                                == 25);
static_assert(AUTH_LOGIN_SERVER_NOT_FOUND                            == 26);
static_assert(AUTH_WAIT_QUEUE                                        == 27);
static_assert(AUTH_BANNED                                            == 28);
static_assert(AUTH_ALREADY_ONLINE                                    == 29);
static_assert(AUTH_NO_TIME                                           == 30);
static_assert(AUTH_DB_BUSY                                           == 31);
static_assert(AUTH_SUSPENDED                                         == 32);
static_assert(AUTH_PARENTAL_CONTROL                                  == 33);
static_assert(AUTH_LOCKED_ENFORCED                                   == 34);
static_assert(REALM_LIST_IN_PROGRESS                                 == 35);
static_assert(REALM_LIST_SUCCESS                                     == 36);
static_assert(REALM_LIST_FAILED                                      == 37);
static_assert(REALM_LIST_INVALID                                     == 38);
static_assert(REALM_LIST_REALM_NOT_FOUND                             == 39);
static_assert(ACCOUNT_CREATE_IN_PROGRESS                             == 40);
static_assert(ACCOUNT_CREATE_SUCCESS                                 == 41);
static_assert(ACCOUNT_CREATE_FAILED                                  == 42);
static_assert(CHAR_LIST_RETRIEVING                                   == 43);
static_assert(CHAR_LIST_RETRIEVED                                    == 44);
static_assert(CHAR_LIST_FAILED                                       == 45);
static_assert(CHAR_CREATE_IN_PROGRESS                                == 46);
static_assert(CHAR_CREATE_SUCCESS                                    == 47);
static_assert(CHAR_CREATE_ERROR                                      == 48);
static_assert(CHAR_CREATE_FAILED                                     == 49);
static_assert(CHAR_CREATE_NAME_IN_USE                                == 50);
static_assert(CHAR_CREATE_DISABLED                                   == 51);
static_assert(CHAR_CREATE_PVP_TEAMS_VIOLATION                        == 52);
static_assert(CHAR_CREATE_SERVER_LIMIT                               == 53);
static_assert(CHAR_CREATE_ACCOUNT_LIMIT                              == 54);
static_assert(CHAR_CREATE_SERVER_QUEUE                               == 55);
static_assert(CHAR_CREATE_ONLY_EXISTING                              == 56);
static_assert(CHAR_CREATE_EXPANSION                                  == 57);
static_assert(CHAR_CREATE_EXPANSION_CLASS                            == 58);
static_assert(CHAR_CREATE_LEVEL_REQUIREMENT                          == 59);
static_assert(CHAR_CREATE_UNIQUE_CLASS_LIMIT                         == 60);
static_assert(CHAR_CREATE_CHARACTER_IN_GUILD                         == 61);
static_assert(CHAR_CREATE_RESTRICTED_RACECLASS                       == 62);
static_assert(CHAR_CREATE_CHARACTER_CHOOSE_RACE                      == 63);
static_assert(CHAR_CREATE_CHARACTER_ARENA_LEADER                     == 64);
static_assert(CHAR_CREATE_CHARACTER_DELETE_MAIL                      == 65);
static_assert(CHAR_CREATE_CHARACTER_SWAP_FACTION                     == 66);
static_assert(CHAR_CREATE_CHARACTER_RACE_ONLY                        == 67);
static_assert(CHAR_CREATE_CHARACTER_GOLD_LIMIT                       == 68);
static_assert(CHAR_CREATE_FORCE_LOGIN                                == 69);
static_assert(CHAR_DELETE_IN_PROGRESS                                == 70);
static_assert(CHAR_DELETE_SUCCESS                                    == 71);
static_assert(CHAR_DELETE_FAILED                                     == 72);
static_assert(CHAR_DELETE_FAILED_LOCKED_FOR_TRANSFER                 == 73);
static_assert(CHAR_DELETE_FAILED_GUILD_LEADER                        == 74);
static_assert(CHAR_DELETE_FAILED_ARENA_CAPTAIN                       == 75);
static_assert(CHAR_DELETE_FAILED_HAS_HEIRLOOM_OR_MAIL                == 76);
static_assert(CHAR_LOGIN_IN_PROGRESS                                 == 77);
static_assert(CHAR_LOGIN_SUCCESS                                     == 78);
static_assert(CHAR_LOGIN_NO_WORLD                                    == 79);
static_assert(CHAR_LOGIN_DUPLICATE_CHARACTER                         == 80);
static_assert(CHAR_LOGIN_NO_INSTANCES                                == 81);
static_assert(CHAR_LOGIN_FAILED                                      == 82);
static_assert(CHAR_LOGIN_DISABLED                                    == 83);
static_assert(CHAR_LOGIN_NO_CHARACTER                                == 84);
static_assert(CHAR_LOGIN_LOCKED_FOR_TRANSFER                         == 85);
static_assert(CHAR_LOGIN_LOCKED_BY_BILLING                           == 86);
static_assert(CHAR_LOGIN_LOCKED_BY_MOBILE_AH                         == 87);
static_assert(CHAR_LOGIN_TEMPORARY_GM_LOCK                           == 88);
static_assert(CHAR_NAME_SUCCESS                                      == 89);
static_assert(CHAR_NAME_FAILURE                                      == 90);
static_assert(CHAR_NAME_NO_NAME                                      == 91);
static_assert(CHAR_NAME_TOO_SHORT                                    == 92);
static_assert(CHAR_NAME_TOO_LONG                                     == 93);
static_assert(CHAR_NAME_INVALID_CHARACTER                            == 94);
static_assert(CHAR_NAME_MIXED_LANGUAGES                              == 95);
static_assert(CHAR_NAME_PROFANE                                      == 96);
static_assert(CHAR_NAME_RESERVED                                     == 97);
static_assert(CHAR_NAME_INVALID_APOSTROPHE                           == 98);
static_assert(CHAR_NAME_MULTIPLE_APOSTROPHES                         == 99);
static_assert(CHAR_NAME_THREE_CONSECUTIVE                            == 100);
static_assert(CHAR_NAME_INVALID_SPACE                                == 101);
static_assert(CHAR_NAME_CONSECUTIVE_SPACES                           == 102);
static_assert(CHAR_NAME_RUSSIAN_CONSECUTIVE_SILENT_CHARACTERS        == 103);
static_assert(CHAR_NAME_RUSSIAN_SILENT_CHARACTER_AT_BEGINNING_OR_END == 104);
static_assert(CHAR_NAME_DECLENSION_DOESNT_MATCH_BASE_NAME            == 105);
