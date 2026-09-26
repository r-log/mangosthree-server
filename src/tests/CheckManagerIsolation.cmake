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
#     container, a std::function signature).
# There are no exceptions, and comments are NOT exempt: the rule is simplest when it is text,
# so a comment in these files has to describe the character without those spellings.
# The files are an explicit list, like CheckSyncDb.cmake's converted files, NOT a directory
# scan: the character's own files (Player.h, Player.cpp, Player*.cpp) are to move into
# src/game/entities/player/ later in D4, and they name the class by definition. Each later
# manager PR appends its files here; a listed file that does not exist fails the gate, so a
# rename cannot quietly empty it.
# CMake regexes have no \b, so a word boundary is spelled (^|[^A-Za-z0-9_]).
#
# KNOWN MISSES, stated rather than chased: the rules are text patterns over the raw file, so a
# newline or a comment between the class name and the '*', '&' or '::' after it (the name, then
# '*' on the next line; `Player /**/ *`) is not seen, and neither is the name reached through
# an alias declared in ANOTHER header or assembled by a macro. The CheckHeaderReach rules on
# QuestStatusMgr.h and QuestStatusMgr.cpp close every route that needs the complete type
# (neither may reach Object/Player.h, Object/Unit.h, Server/WorldSession.h or ObjectMgr.h);
# what is left is a pointer through QuestDef.h's forward declaration spelled on purpose to
# dodge a text rule, which review would see.
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
)

# Every ';' in a scanned text becomes this byte first (see scan_text), so no rule has to spell
# ';' -- which would split the CMake lists the rules and their matches are kept in.
string(ASCII 1 SEMI)

set(WORD_START "(^|[^A-Za-z0-9_])")
set(WORD_END "([^A-Za-z0-9_]|$)")
set(INCLUDE_RE "#[ \t]*include[ \t]*[<\"]([^\">]*[/\\\\])?[Pp][Ll][Aa][Yy][Ee][Rr]\\.[Hh][\">]")
set(POINTER_RE "${WORD_START}Player([ \t]+const)?[ \t]*[*&]")
set(MEMBER_RE "${WORD_START}Player[ \t]*::")
set(CLASS_RE "${WORD_START}class[ \t\r\n]+Player${WORD_END}")
set(ALIAS_RE "${WORD_START}(typedef|using)[^A-Za-z0-9_]([^${SEMI}]*[^A-Za-z0-9_])?Player${WORD_END}")
set(TEMPLATE_ARG_RE "<[ \t\r\n]*(const[ \t\r\n]+)?Player${WORD_END}")

set(RULE_NAMES "include of Player.h" "pointer or reference to Player" "member of Player" "class Player"
    "alias naming Player" "Player as a template argument")
set(RULE_RES "${INCLUDE_RE}" "${POINTER_RE}" "${MEMBER_RE}" "${CLASS_RE}" "${ALIAS_RE}" "${TEMPLATE_ARG_RE}")

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
    set(${OUT_VAR} "${FOUND}" PARENT_SCOPE)
endfunction()

# Self-test: every rule against positive and negative text before the real scan. A broken
# regex fails the gate here, with FATAL_ERROR -- it never gets to pass the scan below quietly.
function(expect_hits LABEL TEXT EXPECTED)
    scan_text("${TEXT}" HITS)
    list(LENGTH HITS GOT)
    if(NOT GOT EQUAL EXPECTED)
        message(FATAL_ERROR
            "ManagerIsolation self-test failed (${LABEL}): '${TEXT}' gave ${GOT} hit(s), expected ${EXPECTED}: ${HITS}")
    endif()
endfunction()

expect_hits("include, quoted" "#include \"Player.h\"" 1)
expect_hits("include, angled with a path" "#include <Object/Player.h>" 1)
expect_hits("include, relative with spaces" "#  include   \"../../Object/Player.h\"" 1)
expect_hits("include of a longer name is fine" "#include \"PlayerPetCache.h\"" 0)
expect_hits("include of a prefixed name is fine" "#include \"GamePlayer.h\"" 0)
expect_hits("include, lower case" "#include \"player.h\"" 1)
expect_hits("include, upper case with a path" "#include <OBJECT/PLAYER.H>" 1)
expect_hits("include of a longer name in lower case is fine" "#include \"playerpetcache.h\"" 0)
expect_hits("pointer" "void Bind(Player* owner);" 1)
expect_hits("pointer with a space" "Player *owner = NULL;" 1)
expect_hits("reference" "void Apply(Player& owner);" 1)
expect_hits("east-const pointer" "Player const* owner" 1)
expect_hits("west-const reference" "const Player& owner" 1)
expect_hits("cast" "((Player*)unit)->X();" 1)
expect_hits("longer identifier with a pointer is fine" "PlayerPetCache* cache;" 0)
expect_hits("prefixed identifier with a pointer is fine" "MyPlayer* p;" 0)
expect_hits("member" "Player::GetQuestStatus(1)" 1)
expect_hits("member with a space" "Player ::X" 1)
expect_hits("longer identifier member is fine" "PlayerPetCache::Row" 0)
expect_hits("forward declaration" "class Player;" 1)
expect_hits("definition" "class Player : public Unit" 1)
expect_hits("class at the end of the text" "class Player" 1)
expect_hits("longer class name is fine" "class PlayerPetCache;" 0)
expect_hits("the word in prose is fine" "the owning player object keeps the slots" 0)
expect_hits("capitalised word without punctuation is fine" "Player data stays elsewhere" 0)
expect_hits("two rules in one text" "class Player; Player* p;" 2)
expect_hits("typedef alias" "typedef Player Owner;" 1)
expect_hits("using alias" "using Owner = Player;" 1)
expect_hits("using alias over two lines" "using Owner =\n    Player;" 1)
expect_hits("typedef of a pointer is two rules" "typedef Player* OwnerPtr;" 2)
expect_hits("typedef of a longer name is fine" "typedef PlayerPetCache Cache;" 0)
expect_hits("typedef of something else is fine" "typedef std::set<uint32> QuestSet;" 0)
expect_hits("the alias rule stops at the semicolon" "typedef int Count; Player data stays elsewhere" 0)
expect_hits("a word ending in using is not the keyword" "causing Player trouble" 0)
expect_hits("smart pointer" "std::unique_ptr<Player> owner" 1)
expect_hits("smart pointer, const and spaces" "std::shared_ptr< const Player > owner" 1)
expect_hits("container of pointers is two rules" "std::vector<Player*> owners" 2)
expect_hits("template argument of a longer name is fine" "std::vector<PlayerPetCache> caches" 0)
expect_hits("a std::function signature without the class is fine" "std::function<Quest const*(uint32)> lookup" 0)

# The list itself: never empty, and every entry must exist. A renamed or deleted manager file
# fails here rather than dropping out of the scan.
list(LENGTH MANAGER_FILES FILE_COUNT)
if(FILE_COUNT EQUAL 0)
    message(FATAL_ERROR "ManagerIsolation: MANAGER_FILES is empty -- the gate would check nothing")
endif()
set(MISSING "")
foreach(FILE_REL IN LISTS MANAGER_FILES)
    if(NOT EXISTS "${GAME_DIR}/${FILE_REL}" OR IS_DIRECTORY "${GAME_DIR}/${FILE_REL}")
        list(APPEND MISSING "${FILE_REL}")
    endif()
endforeach()
if(MISSING)
    string(REPLACE ";" "\n  " REPORT "${MISSING}")
    message(FATAL_ERROR
        "ManagerIsolation self-test failed: a listed manager file does not exist (renamed or deleted?):\n  ${REPORT}\n"
        "Update MANAGER_FILES in src/tests/CheckManagerIsolation.cmake to the file's new path.")
endif()

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
        "A character manager names the character class (decoupling D4a):\n  ${REPORT}\n"
        "Pass what the manager needs as parameters (guid, name, time, a lookup); comments are not exempt.")
endif()

message(STATUS "manager isolation: ${FILE_COUNT} files clean, self-test OK")
