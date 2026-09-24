# Decoupling D5a: the aura and spell packet code asks Player, not the session, whether the
# player is loading or logging out, and Player for direct sends. This gate holds the seam so a
# later change cannot grow it back: SpellAuraPeriodic.cpp and UnitAura.cpp must not call
# GetSession(...) at all; SpellPackets.cpp may call it only for GetSessionDbLocaleIndex() and
# must not call SendPacket(...) (SendDirectMessage() replaces it); UnitVisibility.cpp may call
# it only for GetSecurity(). None of the four files may call PlayerLoading(...) or
# PlayerLogout(...) directly -- Player::IsLoading()/IsLoggingOut() replace them. Aliasing
# (WorldSession* s = x->GetSession();) fails the GetSession rule by construction: the alias
# expression still matches "GetSession[ \t]*\(".
# Run standalone (-P), this script sees none of the top-level project's policies. The project
# requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SessionSeam: -DSOURCE_ROOT=<repo root> is required")
endif()

set(GAME_DIR "${SOURCE_ROOT}/src/game")

# file name -> path, in the order the brief lists them
set(SEAM_FILES
    "${GAME_DIR}/WorldHandlers/SpellAuraPeriodic.cpp"
    "${GAME_DIR}/WorldHandlers/SpellPackets.cpp"
    "${GAME_DIR}/Object/UnitAura.cpp"
    "${GAME_DIR}/Object/UnitVisibility.cpp")

set(SESSION_CALL_RE "GetSession[ \t]*\\(")
set(SEND_PACKET_RE "SendPacket[ \t]*\\(")
set(PLAYER_LOADING_RE "PlayerLoading[ \t]*\\(")
set(PLAYER_LOGOUT_RE "PlayerLogout[ \t]*\\(")
set(LOCALE_ALLOW_RE "GetSession\\(\\)->GetSessionDbLocaleIndex\\(\\)")
set(SECURITY_ALLOW_RE "GetSession\\(\\)->GetSecurity\\(\\)")

# Self-test: each regex is exercised against one positive and one negative string before the
# scan runs. A broken regex fails the gate here, with FATAL_ERROR -- it never gets a chance to
# silently pass the real scan below.
function(assert_regex LABEL LINE PATTERN EXPECT_MATCH)
    if(LINE MATCHES "${PATTERN}")
        set(GOT ON)
    else()
        set(GOT OFF)
    endif()
    if(NOT GOT STREQUAL EXPECT_MATCH)
        message(FATAL_ERROR
            "SessionSeam self-test failed (${LABEL}): '${PATTERN}' against '${LINE}' "
            "expected match=${EXPECT_MATCH}, got ${GOT}")
    endif()
endfunction()

assert_regex("GetSession positive" "WorldSession* s = player->GetSession();" "${SESSION_CALL_RE}" ON)
assert_regex("GetSession negative" "player->IsLoading()" "${SESSION_CALL_RE}" OFF)
assert_regex("GetSession does not fire on a longer identifier" "target->GetSessionDbLocaleIndex()" "${SESSION_CALL_RE}" OFF)

assert_regex("SendPacket positive" "GetSession()->SendPacket(&data)" "${SEND_PACKET_RE}" ON)
assert_regex("SendPacket negative" "SendDirectMessage(&data)" "${SEND_PACKET_RE}" OFF)

assert_regex("PlayerLoading positive" "((Player*)x)->GetSession()->PlayerLoading()" "${PLAYER_LOADING_RE}" ON)
assert_regex("PlayerLoading negative" "((Player*)x)->IsLoading()" "${PLAYER_LOADING_RE}" OFF)

assert_regex("PlayerLogout positive" "((Player*)x)->GetSession()->PlayerLogout()" "${PLAYER_LOGOUT_RE}" ON)
assert_regex("PlayerLogout negative" "((Player*)x)->IsLoggingOut()" "${PLAYER_LOGOUT_RE}" OFF)

assert_regex("locale allow matches its exact line" "target->GetSession()->GetSessionDbLocaleIndex()" "${LOCALE_ALLOW_RE}" ON)
assert_regex("locale allow rejects a near-miss" "target->GetSession()->GetSessionDbLocaleIndex2()" "${LOCALE_ALLOW_RE}" OFF)

assert_regex("security allow matches its exact line" "((Player*)this)->GetSession()->GetSecurity()" "${SECURITY_ALLOW_RE}" ON)
assert_regex("security allow rejects a near-miss" "((Player*)this)->GetSession()->GetSecurityLevel()" "${SECURITY_ALLOW_RE}" OFF)

# Whether a line calls GetSession(...) outside the one allowed idiom for its file. The allowed
# text is removed first (string(REPLACE) takes every occurrence on the line, not just the
# first), then the GetSession rule runs on what is left -- so a second, disallowed GetSession(...)
# packed onto the same physical line as the allowed call is still caught. Checking the raw line
# for "does it contain the allowed substring anywhere" (the bug this replaces) would shield that
# second call: the substring test is true for the whole line even when GetSession appears twice.
function(session_line_violates FILE_NAME LINE OUT_VAR)
    set(RESULT OFF)
    if(LINE MATCHES "${SESSION_CALL_RE}")
        set(STRIPPED "${LINE}")
        if(FILE_NAME STREQUAL "SpellPackets.cpp")
            string(REPLACE "GetSession()->GetSessionDbLocaleIndex()" "" STRIPPED "${STRIPPED}")
        endif()
        if(FILE_NAME STREQUAL "UnitVisibility.cpp")
            string(REPLACE "GetSession()->GetSecurity()" "" STRIPPED "${STRIPPED}")
        endif()
        if(STRIPPED MATCHES "${SESSION_CALL_RE}")
            set(RESULT ON)
        endif()
    endif()
    set(${OUT_VAR} "${RESULT}" PARENT_SCOPE)
endfunction()

session_line_violates("SpellPackets.cpp" "GetSession()->GetSessionDbLocaleIndex(); x->GetSession()->GetSecurity();" PACKED_VIOLATES)
if(NOT PACKED_VIOLATES)
    message(FATAL_ERROR
        "SessionSeam self-test failed (a second GetSession(...) packed onto an allowed line): "
        "expected a violation, got none")
endif()

session_line_violates("SpellPackets.cpp" "target->GetSession()->GetSessionDbLocaleIndex()" ALONE_VIOLATES)
if(ALONE_VIOLATES)
    message(FATAL_ERROR
        "SessionSeam self-test failed (the allowed line alone): expected no violation, got one")
endif()

# The real scan.
set(VIOLATIONS "")
foreach(FILE_PATH IN LISTS SEAM_FILES)
    if(NOT EXISTS "${FILE_PATH}")
        message(FATAL_ERROR "SessionSeam: ${FILE_PATH} does not exist")
    endif()

    get_filename_component(FILE_NAME "${FILE_PATH}" NAME)

    file(STRINGS "${FILE_PATH}" LINES)
    set(LINE_NO 0)
    foreach(LINE IN LISTS LINES)
        math(EXPR LINE_NO "${LINE_NO} + 1")

        if(LINE MATCHES "${PLAYER_LOADING_RE}")
            list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: PlayerLoading(...) -- use Player::IsLoading()")
        endif()
        if(LINE MATCHES "${PLAYER_LOGOUT_RE}")
            list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: PlayerLogout(...) -- use Player::IsLoggingOut()")
        endif()

        session_line_violates("${FILE_NAME}" "${LINE}" LINE_VIOLATES)
        if(LINE_VIOLATES)
            list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: GetSession(...) -- the seam forwards through Player instead")
        endif()

        if(FILE_NAME STREQUAL "SpellPackets.cpp" AND LINE MATCHES "${SEND_PACKET_RE}")
            list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: SendPacket(...) -- use Player::SendDirectMessage()")
        endif()
    endforeach()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
    message(FATAL_ERROR
        "The spells<->session seam (decoupling D5a) is broken:\n  ${REPORT}\n"
        "The aura and spell packet code asks Player (IsLoading/IsLoggingOut/SendDirectMessage),\n"
        "not the session, with two named exceptions (locale index, security level).")
endif()

message(STATUS "session seam: 4 files clean, self-test OK")
