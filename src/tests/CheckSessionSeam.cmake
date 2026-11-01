cmake_minimum_required(VERSION 3.18)

include("${CMAKE_CURRENT_LIST_DIR}/GateGuards.cmake")
gate_require_source_root(SessionSeam)

set(GAME_DIR "${SOURCE_ROOT}/src/game")

set(SEAM_FILES
    "${GAME_DIR}/spells/auras/SpellAuraPeriodic.cpp"
    "${GAME_DIR}/spells/pipeline/SpellReports.cpp"
    "${GAME_DIR}/spells/auras/UnitAura.cpp"
    "${GAME_DIR}/Object/UnitVisibility.cpp")

set(SESSION_CALL_RE "GetSession[ \t]*\\(")
set(SEND_PACKET_RE "SendPacket[ \t]*\\(")
set(PLAYER_LOADING_RE "PlayerLoading[ \t]*\\(")
set(PLAYER_LOGOUT_RE "PlayerLogout[ \t]*\\(")

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

        if(LINE MATCHES "${SESSION_CALL_RE}")
            list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: GetSession(...) -- the seam forwards through Player instead")
        endif()

        if(FILE_NAME STREQUAL "SpellReports.cpp" AND LINE MATCHES "${SEND_PACKET_RE}")
            list(APPEND VIOLATIONS "${FILE_PATH}:${LINE_NO}: SendPacket(...) -- use Player::SendDirectMessage()")
        endif()
    endforeach()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
    message(FATAL_ERROR
        "The spells<->session seam is broken:\n  ${REPORT}\n"
        "The aura and spell packet code asks Player (IsLoading/IsLoggingOut/SendDirectMessage),\n"
        "not the session.")
endif()

message(STATUS "session seam: 4 files clean, self-test OK")
