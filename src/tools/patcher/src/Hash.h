#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace mangos::patcher {

using Sha256Digest = std::array<std::uint8_t, 32>;
using Sha1Digest = std::array<std::uint8_t, 20>;

Sha256Digest Sha256(const std::uint8_t* data, std::size_t len);
Sha256Digest Sha256(const std::vector<std::uint8_t>& data);

Sha1Digest Sha1(const std::uint8_t* data, std::size_t len);
Sha1Digest HmacSha1(const std::vector<std::uint8_t>& key,
                    const std::vector<std::uint8_t>& msg);

std::string ToHex(const std::uint8_t* data, std::size_t len);

template <std::size_t N>
std::string ToHex(const std::array<std::uint8_t, N>& d) {
    return ToHex(d.data(), d.size());
}

// Returns false when the text is not an even-length run of hex digits.
bool FromHex(const std::string& text, std::vector<std::uint8_t>& out);

}  // namespace mangos::patcher
