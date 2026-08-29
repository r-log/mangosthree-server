#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "PeImage.h"

namespace mangos::patcher {

// A code site the patcher either rewrites or refuses to touch. `expect` is the
// stock byte run at `va`; `patch` (when non-empty) replaces its first bytes.
struct CodeSite {
    std::string name;
    std::uint64_t va = 0;
    std::vector<std::uint8_t> expect;
    std::vector<std::uint8_t> patch;
};

// A blob located by its stock content, then cross-checked against the virtual
// address the disassembly cites.
struct DataSite {
    std::string name;
    std::uint64_t expect_va = 0;
    std::vector<std::uint8_t> stock;
};

struct Target {
    std::string label;
    Machine machine = Machine::Unknown;
    std::string stock_sha256;

    DataSite modulus;   // connection-redirect RSA modulus, 256 B little-endian
    DataSite seed;      // HMAC key SEED64
    DataSite digest;    // DIGEST20, HMAC-A of the stock auth blob
    CodeSite launcher;  // manifest / Launcher.exe check

    // Sites that must stay stock. A client with these rewritten is a
    // MaNGOSPatcher client: the recv gate is dead and every opcode is forced
    // onto stream 0, which disables the dual-stream architecture.
    std::vector<CodeSite> forbidden;
};

const std::vector<Target>& KnownTargets();
const Target* FindTarget(Machine machine);

// Digest the client checks: HMAC-SHA1(SEED64, auth73 || auth73).
std::vector<std::uint8_t> StockSeed64();

}  // namespace mangos::patcher
