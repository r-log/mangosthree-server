# Decoupling D4a: the character's managers never name the character class. A manager owns its
# state and takes everything else -- guid, name, game time, template lookups -- as parameters,
# which is what lets mangos_tests build one without a live character
# (src/tests/QuestStatusMgrTest.cpp). The moment one of these files includes the character
# header or names the class, that stops being true, so every file in MANAGER_FILES fails on:
#   - an #include of Player.h, in any path form and any letter case ("Player.h",
#     <Object/Player.h>, "../player.h");
#   - the class name followed by '*' or '&' (a pointer or reference to it, with or without
#     spaces between, and with an east `const` between as well: QuestDef.h forward-declares
#     the class, so `... const*` would otherwise compile with no include and no declaration);
#   - the class name followed by '::' (a member of it);
#   - the words `class` and the class name (a forward declaration or a definition);
#   - `typedef` or `using` followed, before the next ';', by the class name as a whole word
#     (an alias would carry the name into the file under another spelling);
#   - '<', an optional `const`, then the class name (a template argument: a smart pointer, a
#     container, a std::function signature);
#   - decoupling D4b: a quoted or angled path ending in Player.h, any letter case, anywhere --
#     not only after `#include`. It catches the include the rule above cannot see: a comment
#     between '#' and `include`, `#include` split by a backslash-newline, and a computed
#     include (`#define H "Player.h"` then `#include H`). It reads the text after lines are
#     spliced and BEFORE literals are blanked, since a quoted path is a string literal;
#   - decoupling D4b: the class name as a WORD anywhere outside a string or character literal.
#     This is the rule that closes what the ones above cannot see and what still compiles
#     against QuestDef.h's forward declaration: `friend Player;`, a by-value parameter or
#     return type in a declaration, a non-first or default template argument, the name split
#     from its '*', '&' or '::' by a newline or a comment, and the name split by a
#     backslash-newline (lines are spliced first, as the compiler does). The rules above stay
#     for their clearer messages, so one spelling usually gives two hits.
# There are no exceptions, and comments are NOT exempt: the rules are simplest when they are
# text, so a comment in these files has to describe the character without those spellings
# ("the character", "the owner"). String and character literals ARE blanked before the word
# rule -- and only for it -- because what a log line prints is behaviour, not a dependency:
# QuestStatusMgr.cpp's invalid-status line still says "Player %s have invalid quest ...".
# The lexer that blanks them takes comments first, so a quote or an apostrophe inside a
# comment never opens a literal that would hide the rest of the line.
#
# The files are an explicit list, like CheckSyncDb.cmake's converted files, NOT a directory
# scan: the character's own files (Player.h, Player.cpp, Player*.cpp) are to move into
# src/game/entities/player/ later in D4 (D4j), and they name the class by definition. Each
# later manager PR appends its files to MANAGER_FILES; D4j puts the character's own files in
# OWNER_FILES. Decoupling D4b: EVERY file under src/game/entities/player/ must be on one of the
# two lists, so a new manager file that nobody listed fails here instead of going unscanned; a
# listed file that does not exist fails as well, so a rename cannot quietly empty either list.
# CMake regexes have no \b, so a word boundary is spelled (^|[^A-Za-z0-9_]).
#
# KNOWN MISSES, stated rather than chased: the rules are text, so they cannot see the class
# reached without its name being spelled in the file -- through an alias or a macro declared in
# ANOTHER header, through `auto`/`decltype` of a call declared elsewhere that returns it, or
# through the preprocessor assembling the name (token pasting `Pla ## yer`) -- nor the name
# spelled with universal character names (`\u0050layer`, the P written as its code point).
# Two lexer shortcuts also blank text that is code: a character literal is at most 8
# characters between apostrophes, so C++14 digit separators (`1'000'000`) pair up as literals,
# and a raw string literal is read as an ordinary one. For the header itself: a computed
# include whose macro is defined in ANOTHER header (`#include SOME_MACRO`, the path never
# spelled here), and an include of a different header that includes Player.h in turn, are not
# seen by this gate; the second is what CheckHeaderReach is for, and the first it cannot see
# either (it only follows spelled paths). The CheckHeaderReach rules on QuestStatusMgr.h and
# QuestStatusMgr.cpp close every route that needs the complete type (neither may reach
# Object/Player.h, Object/Unit.h, Server/WorldSession.h or ObjectMgr.h); what is left is a
# pointer through QuestDef.h's forward declaration spelled on purpose to dodge a text rule,
# which review would see.
# Run standalone (-P), this script sees none of the top-level project's policies. The project
# requires CMake >= 3.18, so that is also the floor here.
cmake_minimum_required(VERSION 3.18)

if(NOT DEFINED SOURCE_ROOT)
    message(FATAL_ERROR "ManagerIsolation: -DSOURCE_ROOT=<repo root> is required")
endif()

set(GAME_DIR "${SOURCE_ROOT}/src/game")

# Paths relative to src/game.
set(MANAGER_FILES
    entities/player/QuestStatusMgr.h                        # decoupling D4a
    entities/player/QuestStatusMgr.cpp                      # decoupling D4a
    entities/player/TalentMgr.h                             # decoupling D4c
    entities/player/TalentMgr.cpp                           # decoupling D4c
)

# The character's own files once they live under entities/player/ (D4j). They name the class by
# definition, so they are not scanned; being on this list is what lets them sit in the directory.
set(OWNER_FILES
)

# Every file under this directory (relative to src/game) must be on one of the two lists.
set(LISTED_DIR entities/player)

# Every ';' in a scanned text becomes this byte first (see scan_text), so no rule has to spell
# ';' -- which would split the CMake lists the rules and their matches are kept in.
string(ASCII 1 SEMI)
# Placeholders the literal lexer (blank_literals) swaps in for the two-character sequences that
# would otherwise need a regex alternation inside a '*' -- an escaped backslash, an escaped
# double quote, an escaped apostrophe, and the end of a block comment -- so every repeated part
# of its regex is a plain character class. They are swapped back before the rule runs.
string(ASCII 2 ESC_BACKSLASH)
string(ASCII 3 ESC_DQUOTE)
string(ASCII 4 ESC_SQUOTE)
string(ASCII 5 COMMENT_END)

set(WORD_START "(^|[^A-Za-z0-9_])")
set(WORD_END "([^A-Za-z0-9_]|$)")
set(INCLUDE_RE "#[ \t]*include[ \t]*[<\"]([^\">]*[/\\\\])?[Pp][Ll][Aa][Yy][Ee][Rr]\\.[Hh][\">]")
set(POINTER_RE "${WORD_START}Player([ \t]+const)?[ \t]*[*&]")
set(MEMBER_RE "${WORD_START}Player[ \t]*::")
set(CLASS_RE "${WORD_START}class[ \t\r\n]+Player${WORD_END}")
set(ALIAS_RE "${WORD_START}(typedef|using)[^A-Za-z0-9_]([^${SEMI}]*[^A-Za-z0-9_])?Player${WORD_END}")
set(TEMPLATE_ARG_RE "<[ \t\r\n]*(const[ \t\r\n]+)?Player${WORD_END}")
set(PATH_RE "[<\"]([^\">]*[/\\\\])?[Pp][Ll][Aa][Yy][Ee][Rr]\\.[Hh][>\"]")
set(WORD_RE "${WORD_START}Player${WORD_END}")

# The rules that read the raw text. The path rule reads the text with its lines spliced; the
# word rule reads the spliced text with its literals blanked.
set(RULE_NAMES "include of Player.h" "pointer or reference to Player" "member of Player" "class Player"
    "alias naming Player" "Player as a template argument")
set(RULE_RES "${INCLUDE_RE}" "${POINTER_RE}" "${MEMBER_RE}" "${CLASS_RE}" "${ALIAS_RE}" "${TEMPLATE_ARG_RE}")
set(PATH_RULE_NAME "a path naming Player.h")
set(WORD_RULE_NAME "the word Player outside a literal")

# One token of the literal lexer: a line comment, a block comment, a string literal, or a
# character literal of at most 8 characters. Comments come first in the alternation and the
# search is leftmost, so whichever starts first wins -- the order a compiler reads them in.
set(CHAR_BODY "[^'\n]?[^'\n]?[^'\n]?[^'\n]?[^'\n]?[^'\n]?[^'\n]?[^'\n]?")
set(LEX_RE "//[^\n]*|/\\*[^${COMMENT_END}]*${COMMENT_END}|\"[^\"\n]*\"|'${CHAR_BODY}'")

# TEXT with every backslash-newline removed, as translation phase 2 does: once, in one pass (a
# splice that forms a new backslash-newline is not spliced again), so it runs once per text.
function(splice_lines TEXT OUT_VAR)
    string(REGEX REPLACE "\\\\\r?\n" "" TEXT "${TEXT}")
    set(${OUT_VAR} "${TEXT}" PARENT_SCOPE)
endfunction()

# The spliced TEXT with every string literal emptied to "" and every character literal to '',
# comments and code kept as they are.
function(blank_literals TEXT OUT_VAR)
    string(REPLACE "\\\\" "${ESC_BACKSLASH}" TEXT "${TEXT}")
    string(REPLACE "\\\"" "${ESC_DQUOTE}" TEXT "${TEXT}")
    string(REPLACE "\\'" "${ESC_SQUOTE}" TEXT "${TEXT}")
    string(REPLACE "*/" "${COMMENT_END}" TEXT "${TEXT}")
    # string(REGEX REPLACE) cannot keep a comment and empty a literal in one pass (an
    # alternative's group that did not take part is an error there), so the tokens are walked.
    set(RESULT "")
    while(TRUE)
        string(REGEX MATCH "${LEX_RE}" TOKEN "${TEXT}")
        if("${TOKEN}" STREQUAL "")
            break()
        endif()
        # The first occurrence of the token's text is where the leftmost match starts: an
        # earlier copy would have been matched first.
        string(FIND "${TEXT}" "${TOKEN}" AT)
        string(SUBSTRING "${TEXT}" 0 ${AT} BEFORE)
        string(APPEND RESULT "${BEFORE}")
        string(SUBSTRING "${TOKEN}" 0 1 FIRST)
        if(FIRST STREQUAL "\"")
            string(APPEND RESULT "\"\"")
        elseif(FIRST STREQUAL "'")
            string(APPEND RESULT "''")
        else()
            string(APPEND RESULT "${TOKEN}")
        endif()
        string(LENGTH "${TOKEN}" LENGTH)
        math(EXPR NEXT "${AT} + ${LENGTH}")
        string(SUBSTRING "${TEXT}" ${NEXT} -1 TEXT)
    endwhile()
    string(APPEND RESULT "${TEXT}")
    string(REPLACE "${ESC_BACKSLASH}" "\\\\" RESULT "${RESULT}")
    string(REPLACE "${ESC_DQUOTE}" "\\\"" RESULT "${RESULT}")
    string(REPLACE "${ESC_SQUOTE}" "\\'" RESULT "${RESULT}")
    string(REPLACE "${COMMENT_END}" "*/" RESULT "${RESULT}")
    set(${OUT_VAR} "${RESULT}" PARENT_SCOPE)
endfunction()

# The violations in one text, as "<rule>: <matched text>" entries.
function(scan_text TEXT OUT_VAR)
    # A ';' in a match would split the CMake list of matches in two, so every ';' becomes the
    # SEMI byte first: the alias rule stops at it, and the other rules see it as the non-word
    # character it was.
    string(REPLACE ";" "${SEMI}" TEXT "${TEXT}")
    set(FOUND "")
    list(LENGTH RULE_NAMES RULE_COUNT)
    math(EXPR LAST "${RULE_COUNT} - 1")
    foreach(I RANGE ${LAST})
        list(GET RULE_NAMES ${I} NAME)
        list(GET RULE_RES ${I} RE)
        string(REGEX MATCHALL "${RE}" HITS "${TEXT}")
        foreach(HIT IN LISTS HITS)
            string(REPLACE "${SEMI}" " " HIT "${HIT}")
            string(STRIP "${HIT}" HIT)
            list(APPEND FOUND "${NAME}: '${HIT}'")
        endforeach()
    endforeach()
    splice_lines("${TEXT}" SPLICED)
    string(REGEX MATCHALL "${PATH_RE}" HITS "${SPLICED}")
    foreach(HIT IN LISTS HITS)
        string(REPLACE "${SEMI}" " " HIT "${HIT}")
        string(STRIP "${HIT}" HIT)
        list(APPEND FOUND "${PATH_RULE_NAME}: '${HIT}'")
    endforeach()
    blank_literals("${SPLICED}" CODE)
    string(REGEX MATCHALL "${WORD_RE}" HITS "${CODE}")
    foreach(HIT IN LISTS HITS)
        string(REPLACE "${SEMI}" " " HIT "${HIT}")
        string(STRIP "${HIT}" HIT)
        list(APPEND FOUND "${WORD_RULE_NAME}: '${HIT}'")
    endforeach()
    set(${OUT_VAR} "${FOUND}" PARENT_SCOPE)
endfunction()

# The files under LISTED_DIR that neither list names (UNLISTED_OUT), and the files both lists
# name (TWICE_OUT). The three inputs are the NAMES of list variables.
function(check_listing PRESENT_VAR MANAGERS_VAR OWNERS_VAR UNLISTED_OUT TWICE_OUT)
    set(UNLISTED "")
    foreach(FILE_REL IN LISTS ${PRESENT_VAR})
        if(NOT FILE_REL IN_LIST ${MANAGERS_VAR} AND NOT FILE_REL IN_LIST ${OWNERS_VAR})
            list(APPEND UNLISTED "${FILE_REL}")
        endif()
    endforeach()
    set(TWICE "")
    foreach(FILE_REL IN LISTS ${MANAGERS_VAR})
        if(FILE_REL IN_LIST ${OWNERS_VAR})
            list(APPEND TWICE "${FILE_REL}")
        endif()
    endforeach()
    set(${UNLISTED_OUT} "${UNLISTED}" PARENT_SCOPE)
    set(${TWICE_OUT} "${TWICE}" PARENT_SCOPE)
endfunction()

# Self-test: every rule against positive and negative text before the real scan. A broken
# regex fails the gate here, with FATAL_ERROR -- it never gets to pass the scan below quietly.
# The count is the number of hits over all the rules: the word rule adds one to every spelling
# of the name outside a literal, next to the specific rule that names it.
function(expect_hits LABEL TEXT EXPECTED)
    scan_text("${TEXT}" HITS)
    list(LENGTH HITS GOT)
    if(NOT GOT EQUAL EXPECTED)
        message(FATAL_ERROR
            "ManagerIsolation self-test failed (${LABEL}): '${TEXT}' gave ${GOT} hit(s), expected ${EXPECTED}: ${HITS}")
    endif()
endfunction()

# The include rule, each with the path rule. A quoted include is a string literal, so the word
# rule does not see it; an angled one it does.
expect_hits("include, quoted" "#include \"Player.h\"" 2)
expect_hits("include, angled with a path (and the word)" "#include <Object/Player.h>" 3)
expect_hits("include, relative with spaces" "#  include   \"../../Object/Player.h\"" 2)
expect_hits("include of a longer name is fine" "#include \"PlayerPetCache.h\"" 0)
expect_hits("include of a prefixed name is fine" "#include \"GamePlayer.h\"" 0)
expect_hits("include, lower case" "#include \"player.h\"" 2)
expect_hits("include, upper case with a path" "#include <OBJECT/PLAYER.H>" 2)
expect_hits("include of a longer name in lower case is fine" "#include \"playerpetcache.h\"" 0)
# Decoupling D4b, the path rule alone: the includes the include rule cannot see.
expect_hits("a comment between the hash and include" "# /* x */ include \"Player.h\"" 1)
expect_hits("include split by a backslash-newline" "#inc\\\nlude \"Player.h\"" 1)
expect_hits("a computed include" "#define H \"Player.h\"\n#include H" 1)
expect_hits("the path split by a backslash-newline" "#include \"Pla\\\nyer.h\"" 1)
expect_hits("a backslash path separator (and the word)" "#include <Object\\Player.h>" 3)
expect_hits("a longer extension is fine" "#include \"Player.hpp\"" 0)
expect_hits("a prefixed file name is fine" "#include \"../MyPlayer.h\"" 0)
expect_hits("the manager's own log literal is fine"
    "sLog.outError(\"Player %s have invalid quest %d status (%d), replaced by QUEST_STATUS_NONE(0).\", ownerName, quest_id, qstatus);" 0)
# The pointer, member and class rules, each with the word.
expect_hits("pointer" "void Bind(Player* owner);" 2)
expect_hits("pointer with a space" "Player *owner = NULL;" 2)
expect_hits("reference" "void Apply(Player& owner);" 2)
expect_hits("east-const pointer" "Player const* owner" 2)
expect_hits("west-const reference" "const Player& owner" 2)
expect_hits("cast" "((Player*)unit)->X();" 2)
expect_hits("longer identifier with a pointer is fine" "PlayerPetCache* cache;" 0)
expect_hits("prefixed identifier with a pointer is fine" "MyPlayer* p;" 0)
expect_hits("member" "Player::GetQuestStatus(1)" 2)
expect_hits("member with a space" "Player ::X" 2)
expect_hits("longer identifier member is fine" "PlayerPetCache::Row" 0)
expect_hits("forward declaration" "class Player;" 2)
expect_hits("definition" "class Player : public Unit" 2)
expect_hits("class at the end of the text" "class Player" 2)
expect_hits("longer class name is fine" "class PlayerPetCache;" 0)
expect_hits("the lower-case word in prose is fine" "the owning player object keeps the slots" 0)
expect_hits("two rules and two words in one text" "class Player; Player* p;" 4)
# The alias and template-argument rules, each with the word.
expect_hits("typedef alias" "typedef Player Owner;" 2)
expect_hits("using alias" "using Owner = Player;" 2)
expect_hits("using alias over two lines" "using Owner =\n    Player;" 2)
expect_hits("typedef of a pointer is two rules and the word" "typedef Player* OwnerPtr;" 3)
expect_hits("typedef of a longer name is fine" "typedef PlayerPetCache Cache;" 0)
expect_hits("typedef of something else is fine" "typedef std::set<uint32> QuestSet;" 0)
expect_hits("the alias rule stops at the semicolon (the word is still caught)" "typedef int Count; Player data" 1)
expect_hits("a word ending in using is not the keyword (the word is still caught)" "causing Player trouble" 1)
expect_hits("smart pointer" "std::unique_ptr<Player> owner" 2)
expect_hits("smart pointer, const and spaces" "std::shared_ptr< const Player > owner" 2)
expect_hits("container of pointers is two rules and the word" "std::vector<Player*> owners" 3)
expect_hits("template argument of a longer name is fine" "std::vector<PlayerPetCache> caches" 0)
expect_hits("a std::function signature without the class is fine" "std::function<Quest const*(uint32)> lookup" 0)
# Decoupling D4b, the word rule: the spellings that compile against a forward declaration and
# that no rule above sees.
expect_hits("friend without class" "friend Player;" 1)
expect_hits("by-value parameter" "void Bind(Player owner);" 1)
expect_hits("by-value return" "Player Owner() const;" 1)
expect_hits("non-first template argument" "std::map<uint32, Player> owners;" 1)
expect_hits("non-first template argument, pointer" "std::map<uint32, Player*> owners;" 2)
expect_hits("default template argument" "template <typename T = Player> struct Holder;" 1)
expect_hits("function type argument" "std::function<void(Player const&)> cb;" 2)
expect_hits("the name, then a newline, then the star" "Player\n    * owner;" 1)
expect_hits("the name, then a comment, then the star" "Player /* x */ * owner;" 1)
expect_hits("the name, then a newline, then ::" "Player\n::X();" 1)
expect_hits("the name split by a backslash-newline" "Pla\\\nyer* owner;" 1)
expect_hits("the capitalised word in comment prose" "// the Player keeps the slots" 1)
expect_hits("the word at the start and the end" "Player" 1)
expect_hits("longer names are fine" "PlayerPetCache MyPlayer Player_ Player2 PLAYER player" 0)
# Literals are blanked for the word rule; comments are not.
expect_hits("a string literal is text" "sLog.outError(\"Player %s have invalid quest\");" 0)
expect_hits("escaped quotes stay inside the literal" "Log(\"a \\\"Player\\\" b\");" 0)
expect_hits("an escaped backslash ends before the quote" "Log(\"\\\\\"); Player p;" 1)
expect_hits("a quote in a character literal opens nothing" "char q = '\"'; Player p; char r = '\"';" 1)
expect_hits("an escaped apostrophe character literal" "char a = '\\''; Player p;" 1)
expect_hits("a quoted name in a line comment is still a comment" "// it's the \"Player\" here" 1)
expect_hits("a quoted name in a block comment is still a comment" "/* the \"Player\" */ int x;" 1)
expect_hits("a line comment inside a string is not a comment" "Log(\"//\"); Log(\"Player\");" 0)
expect_hits("a block comment inside a string is not a comment" "Log(\"/*\"); Player p; Log(\"*/\");" 1)
expect_hits("an apostrophe in a comment opens nothing" "// the owner's\nPlayer p; // it's" 1)
expect_hits("empty literals" "Log(\"\"); char c = ' '; Player p;" 1)

# Self-test of the listing check, on made-up paths.
function(expect_listing LABEL PRESENT MANAGERS OWNERS EXPECTED_UNLISTED EXPECTED_TWICE)
    check_listing(PRESENT MANAGERS OWNERS UNLISTED TWICE)
    list(LENGTH UNLISTED GOT_UNLISTED)
    list(LENGTH TWICE GOT_TWICE)
    if(NOT GOT_UNLISTED EQUAL EXPECTED_UNLISTED OR NOT GOT_TWICE EQUAL EXPECTED_TWICE)
        message(FATAL_ERROR
            "ManagerIsolation self-test failed (${LABEL}): ${GOT_UNLISTED} unlisted [${UNLISTED}], "
            "${GOT_TWICE} on both lists [${TWICE}]; expected ${EXPECTED_UNLISTED} and ${EXPECTED_TWICE}")
    endif()
endfunction()

expect_listing("every file on a list" "d/A.h;d/A.cpp;d/Owner.cpp" "d/A.h;d/A.cpp" "d/Owner.cpp" 0 0)
expect_listing("a new manager file nobody listed" "d/A.h;d/A.cpp;d/B.cpp" "d/A.h;d/A.cpp" "" 1 0)
expect_listing("a character file moved in before D4j lists it" "d/A.h;d/Owner.cpp" "d/A.h" "" 1 0)
expect_listing("a file that is not code counts too" "d/A.h;d/README.md" "d/A.h" "" 1 0)
expect_listing("the match is exact, not a prefix" "d/A.h;d/A.hpp" "d/A.h" "" 1 0)
expect_listing("a file on both lists" "d/A.h" "d/A.h" "d/A.h" 0 1)
expect_listing("an empty directory" "" "d/A.h" "" 0 0)

# The lists themselves: MANAGER_FILES never empty, and every entry on either list must exist. A
# renamed or deleted file fails here rather than dropping out of the scan.
list(LENGTH MANAGER_FILES FILE_COUNT)
list(LENGTH OWNER_FILES OWNER_COUNT)
if(FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "ManagerIsolation: MANAGER_FILES is empty -- the gate would check nothing")
endif()
set(MISSING "")
foreach(FILE_REL IN LISTS MANAGER_FILES OWNER_FILES)
    if(NOT EXISTS "${GAME_DIR}/${FILE_REL}" OR IS_DIRECTORY "${GAME_DIR}/${FILE_REL}")
        list(APPEND MISSING "${FILE_REL}")
    endif()
endforeach()
if(MISSING)
    string(REPLACE ";" "\n  " REPORT "${MISSING}")
    message(FATAL_ERROR
        "ManagerIsolation self-test failed: a listed file does not exist (renamed or deleted?):\n  ${REPORT}\n"
        "Update MANAGER_FILES or OWNER_FILES in src/tests/CheckManagerIsolation.cmake to the file's new path.")
endif()

# Every file under LISTED_DIR is on exactly one list.
file(GLOB_RECURSE PRESENT_FILES LIST_DIRECTORIES false RELATIVE "${GAME_DIR}" "${GAME_DIR}/${LISTED_DIR}/*")
check_listing(PRESENT_FILES MANAGER_FILES OWNER_FILES UNLISTED TWICE)
if(UNLISTED OR TWICE)
    set(UNLISTED_REPORT "(none)")
    set(TWICE_REPORT "(none)")
    if(UNLISTED)
        string(REPLACE ";" "\n  " UNLISTED_REPORT "${UNLISTED}")
    endif()
    if(TWICE)
        string(REPLACE ";" "\n  " TWICE_REPORT "${TWICE}")
    endif()
    message(FATAL_ERROR
        "Every file under src/game/${LISTED_DIR}/ must be on exactly one list of src/tests/CheckManagerIsolation.cmake "
        "(decoupling D4b): MANAGER_FILES for a manager (scanned), OWNER_FILES for the character's own files.\n"
        "On neither list:\n  ${UNLISTED_REPORT}\nOn both lists:\n  ${TWICE_REPORT}")
endif()
list(LENGTH PRESENT_FILES PRESENT_COUNT)

# The real scan.
set(VIOLATIONS "")
foreach(FILE_REL IN LISTS MANAGER_FILES)
    set(FILE_PATH "${GAME_DIR}/${FILE_REL}")
    file(READ "${FILE_PATH}" CONTENT)
    scan_text("${CONTENT}" HITS)
    foreach(HIT IN LISTS HITS)
        list(APPEND VIOLATIONS "${FILE_PATH}: ${HIT}")
    endforeach()
endforeach()

if(VIOLATIONS)
    string(REPLACE ";" "\n  " REPORT "${VIOLATIONS}")
    message(FATAL_ERROR
        "A character manager names the character class (decoupling D4a/D4b):\n  ${REPORT}\n"
        "Pass what the manager needs as parameters (guid, name, time, a lookup); comments are not exempt.")
endif()

message(STATUS "manager isolation: ${FILE_COUNT} files clean, ${OWNER_COUNT} owner files, "
    "all ${PRESENT_COUNT} files under ${LISTED_DIR}/ listed, self-test OK")
