# Decoupling D7: a file that has been converted off synchronous database calls stays
# converted. For every path in CONVERTED_FILES, a line that calls one of the blocking
# Database entry points on CharacterDatabase/WorldDatabase/LoginDatabase fails this gate,
# naming the file and the line -- unless that exact line is in the file's allow list.
#
# This gate covers DIRECT calls in converted files. It cannot see an indirect chain (a
# handler that calls a helper that queries), and it is not meant to: the runtime counter
# (TickGuard, `.server database`) is what measures those, because only a run knows which
# chains the tick actually walks. The two together are the proof; neither alone is.
#
# CONVERTED_FILES is EMPTY in D7a, which converts nothing -- the gate is built here so the
# conversion PRs have somewhere to append. The self-tests below still run, so the gate is
# known to work before it has anything to guard.
#
# Run standalone (-P), this script sees none of the top-level project's policies. The
# project requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "SyncDb: -DSOURCE_ROOT=<repo root> is required")
endif()

# Repo-relative paths. Later PRs append one line per converted file.
set(CONVERTED_FILES
)

# Per file, the exact lines (trimmed) that are allowed to keep a direct call -- a startup
# path in a file that is otherwise converted, say. Named ALLOW_<file name with every
# non-alphanumeric character replaced by _>. Empty in D7a.
#
# Write the semicolons in the line text as \; -- an unescaped one would split the entry
# into two list elements, neither of which matches anything. The self-test below covers
# exactly that spelling, so a mistake here fails the gate rather than quietly allowing
# nothing:
#
#   set(ALLOW_PetitionsHandler_cpp
#       "CharacterDatabase.escape_string(name)\;")

set(SYNC_DB_RE "(CharacterDatabase|WorldDatabase|LoginDatabase)[ \t]*\\.[ \t]*(P?Query|QueryNamed|PQueryNamed|DirectExecute|DirectPExecute|DirectExecuteStmt|Ping|CommitTransactionChecked|escape_string)[ \t]*\\(")

# Self-test: the regex is exercised against a positive and a negative string for every
# spelling it has to recognise, and the allow-line rule is exercised both ways, before the
# scan runs. A broken regex fails here, with FATAL_ERROR -- it never gets the chance to
# silently pass the real scan below.
function(assert_regex LABEL LINE PATTERN EXPECT_MATCH)
    if(LINE MATCHES "${PATTERN}")
        set(GOT ON)
    else()
        set(GOT OFF)
    endif()
    if(NOT GOT STREQUAL EXPECT_MATCH)
        message(FATAL_ERROR
            "SyncDb self-test failed (${LABEL}): '${PATTERN}' against '${LINE}' "
            "expected match=${EXPECT_MATCH}, got ${GOT}")
    endif()
endfunction()

assert_regex("Query" "QueryResult* r = CharacterDatabase.Query(\"SELECT 1\");" "${SYNC_DB_RE}" ON)
assert_regex("PQuery" "QueryResult* r = CharacterDatabase.PQuery(\"SELECT %u\", 1);" "${SYNC_DB_RE}" ON)
assert_regex("QueryNamed" "WorldDatabase.QueryNamed(\"SELECT 1\");" "${SYNC_DB_RE}" ON)
assert_regex("PQueryNamed" "WorldDatabase.PQueryNamed(\"SELECT %u\", 1);" "${SYNC_DB_RE}" ON)
assert_regex("DirectExecute" "LoginDatabase.DirectExecute(\"SET NAMES utf8\");" "${SYNC_DB_RE}" ON)
assert_regex("DirectPExecute" "LoginDatabase.DirectPExecute(\"UPDATE x SET y = %u\", 1);" "${SYNC_DB_RE}" ON)
assert_regex("DirectExecuteStmt" "CharacterDatabase.DirectExecuteStmt(id, params);" "${SYNC_DB_RE}" ON)
assert_regex("Ping" "WorldDatabase.Ping();" "${SYNC_DB_RE}" ON)
assert_regex("CommitTransactionChecked" "if (!CharacterDatabase.CommitTransactionChecked())" "${SYNC_DB_RE}" ON)
assert_regex("escape_string" "CharacterDatabase.escape_string(name);" "${SYNC_DB_RE}" ON)
assert_regex("whitespace around the dot and the paren" "CharacterDatabase . PQuery (\"SELECT 1\");" "${SYNC_DB_RE}" ON)

assert_regex("an async query is not a synchronous one" "CharacterDatabase.AsyncPQuery(cb, \"SELECT %u\", 1);" "${SYNC_DB_RE}" OFF)
assert_regex("a queued execute is not a synchronous one" "CharacterDatabase.PExecute(\"UPDATE x SET y = %u\", 1);" "${SYNC_DB_RE}" OFF)
assert_regex("a held query holder is not a synchronous one" "CharacterDatabase.DelayQueryHolder(cb, holder);" "${SYNC_DB_RE}" OFF)
assert_regex("another object's Query is not one of the three" "sObjectMgr.Query(\"SELECT 1\");" "${SYNC_DB_RE}" OFF)
assert_regex("a mention without a call is not a call" "// CharacterDatabase.PQuery is what this replaced" "${SYNC_DB_RE}" OFF)

# Whether a line calls a blocking entry point that its file is not allowed to keep. The
# allowed lines are compared as exact (trimmed) text, so an allowance covers the one line
# it was written for and not a second call bolted onto it.
function(sync_db_line_violates FILE_KEY LINE OUT_VAR)
    set(RESULT OFF)
    if(LINE MATCHES "${SYNC_DB_RE}")
        set(RESULT ON)
        string(STRIP "${LINE}" TRIMMED)
        foreach(ALLOWED IN LISTS ALLOW_${FILE_KEY})
            string(STRIP "${ALLOWED}" ALLOWED_TRIMMED)
            if(TRIMMED STREQUAL ALLOWED_TRIMMED)
                set(RESULT OFF)
            endif()
        endforeach()
    endif()
    set(${OUT_VAR} "${RESULT}" PARENT_SCOPE)
endfunction()

# The allow rule, both ways, on a file key that exists only for this self-test. Note the
# \; -- that is the spelling a real entry has to use.
set(ALLOW_SelfTest_cpp "CharacterDatabase.escape_string(name)\;")
sync_db_line_violates("SelfTest_cpp" "    CharacterDatabase.escape_string(name);" ALLOWED_VIOLATES)
if(ALLOWED_VIOLATES)
    message(FATAL_ERROR
        "SyncDb self-test failed (an allowed line): expected no violation, got one")
endif()
sync_db_line_violates("SelfTest_cpp" "    CharacterDatabase.escape_string(other);" NEARMISS_VIOLATES)
if(NOT NEARMISS_VIOLATES)
    message(FATAL_ERROR
        "SyncDb self-test failed (a line the allowance does not cover): "
        "expected a violation, got none")
endif()
sync_db_line_violates("NoAllowList_cpp" "    CharacterDatabase.escape_string(name);" UNALLOWED_VIOLATES)
if(NOT UNALLOWED_VIOLATES)
    message(FATAL_ERROR
        "SyncDb self-test failed (a file with no allow list): expected a violation, got none")
endif()
unset(ALLOW_SelfTest_cpp)

# The real scan.
set(VIOLATIONS "")
list(LENGTH CONVERTED_FILES CONVERTED_COUNT)
foreach(REL_PATH IN LISTS CONVERTED_FILES)
    set(FILE_PATH "${SOURCE_ROOT}/${REL_PATH}")
    if(NOT EXISTS "${FILE_PATH}")
        message(FATAL_ERROR "SyncDb: ${FILE_PATH} does not exist")
    endif()

    get_filename_component(FILE_NAME "${FILE_PATH}" NAME)
    string(REGEX REPLACE "[^A-Za-z0-9]" "_" FILE_KEY "${FILE_NAME}")

    file(STRINGS "${FILE_PATH}" LINES)
    set(LINE_NO 0)
    foreach(LINE IN LISTS LINES)
        math(EXPR LINE_NO "${LINE_NO} + 1")

        sync_db_line_violates("${FILE_KEY}" "${LINE}" LINE_VIOLATES)
        if(LINE_VIOLATES)
            list(APPEND VIOLATIONS
                "${REL_PATH}:${LINE_NO}: a synchronous database call in a converted file")
        endif()
    endforeach()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
    list(LENGTH VIOLATIONS VIOLATION_COUNT)
    message(FATAL_ERROR
        "The no-sync-DB-in-the-tick seam (decoupling D7) is broken in ${VIOLATION_COUNT} place(s):\n  ${REPORT}\n"
        "A converted file asks the database asynchronously (AsyncQuery/AsyncPQuery/\n"
        "DelayQueryHolder) or defers the write (Execute/PExecute). If one of these lines\n"
        "genuinely runs outside the tick, add its exact text to that file's allow list in\n"
        "src/tests/CheckSyncDb.cmake and say in the PR why.")
endif()

message(STATUS "sync db: ${CONVERTED_COUNT} converted file(s) clean, self-test OK")
