# =============================================================================
# mangos_crypto: the primitives the server's protocols run on, owned by this tree.
#
# No dependency on the rest of the tree -- the C++17 standard library and the OS
# random-number API only -- so a tool that needs to hash a file or sign a block
# links this and nothing else, and every primitive is testable on its own.
# =============================================================================
set(CRYPTO_SOURCES
    Fatal.cpp
    Fatal.h
    Hex.cpp
    Hex.h
    BlockHash.h
    Sha1.cpp
    Sha1.h
    Sha256.cpp
    Sha256.h
    Md5.cpp
    Md5.h
    HmacSha1.cpp
    HmacSha1.h
    SecureZero.cpp
    SecureZero.h
    Limb.h
    BigInt.cpp
    BigInt.h
    SystemRandom.cpp
    SystemRandom.h
    Montgomery.cpp
    Montgomery.h
    MontgomeryX86.cpp
    ModExp.cpp
    ModExp.h
    Rsa.cpp
    Rsa.h
)

add_library(mangos_crypto STATIC ${CRYPTO_SOURCES})

target_include_directories(mangos_crypto
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/shared)

# The OS random source: BCryptGenRandom on Windows; getrandom(2)/arc4random_buf need no library.
target_link_libraries(mangos_crypto PUBLIC $<$<BOOL:${WIN32}>:bcrypt>)

target_compile_features(mangos_crypto PUBLIC cxx_std_17)

if(MSVC)
    target_compile_options(mangos_crypto PRIVATE /W4 /permissive-)
else()
    target_compile_options(mangos_crypto PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(mangos_crypto PROPERTIES FOLDER "shared")
