#include "Hash.h"

#include <cctype>
#include <cstring>

namespace mangos::patcher {
namespace {

constexpr std::uint32_t kSha256K[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u,
    0x923f82a4u, 0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u,
    0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u,
    0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u,
    0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u,
    0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au,
    0x5b9cca4fu, 0x682e6ff3u, 0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u,
    0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

std::uint32_t Ror32(std::uint32_t v, unsigned n) {
    return (v >> n) | (v << (32u - n));
}

std::uint32_t Rol32(std::uint32_t v, unsigned n) {
    return (v << n) | (v >> (32u - n));
}

std::uint32_t LoadBe32(const std::uint8_t* p) {
    return (static_cast<std::uint32_t>(p[0]) << 24) |
           (static_cast<std::uint32_t>(p[1]) << 16) |
           (static_cast<std::uint32_t>(p[2]) << 8) |
           static_cast<std::uint32_t>(p[3]);
}

void StoreBe32(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>(v >> 24);
    p[1] = static_cast<std::uint8_t>(v >> 16);
    p[2] = static_cast<std::uint8_t>(v >> 8);
    p[3] = static_cast<std::uint8_t>(v);
}

void StoreBe64(std::uint8_t* p, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) {
        p[i] = static_cast<std::uint8_t>(v >> (56 - 8 * i));
    }
}

void Sha256Block(std::uint32_t h[8], const std::uint8_t* block) {
    std::uint32_t w[64];
    for (int i = 0; i < 16; ++i) {
        w[i] = LoadBe32(block + 4 * i);
    }
    for (int i = 16; i < 64; ++i) {
        const std::uint32_t s0 = Ror32(w[i - 15], 7) ^ Ror32(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = Ror32(w[i - 2], 17) ^ Ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; ++i) {
        const std::uint32_t s1 = Ror32(e, 6) ^ Ror32(e, 11) ^ Ror32(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = hh + s1 + ch + kSha256K[i] + w[i];
        const std::uint32_t s0 = Ror32(a, 2) ^ Ror32(a, 13) ^ Ror32(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = s0 + maj;
        hh = g; g = f; f = e; e = d + t1;
        d = c; c = b; b = a; a = t1 + t2;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
}

void Sha1Block(std::uint32_t h[5], const std::uint8_t* block) {
    std::uint32_t w[80];
    for (int i = 0; i < 16; ++i) {
        w[i] = LoadBe32(block + 4 * i);
    }
    for (int i = 16; i < 80; ++i) {
        w[i] = Rol32(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    }

    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; ++i) {
        std::uint32_t f = 0;
        std::uint32_t k = 0;
        if (i < 20) {
            f = (b & c) | (~b & d);
            k = 0x5a827999u;
        } else if (i < 40) {
            f = b ^ c ^ d;
            k = 0x6ed9eba1u;
        } else if (i < 60) {
            f = (b & c) | (b & d) | (c & d);
            k = 0x8f1bbcdcu;
        } else {
            f = b ^ c ^ d;
            k = 0xca62c1d6u;
        }
        const std::uint32_t t = Rol32(a, 5) + f + e + k + w[i];
        e = d; d = c; c = Rol32(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}

// Merkle-Damgard tail shared by both digests: 0x80, zero pad, big-endian bit count.
std::vector<std::uint8_t> PadMessage(const std::uint8_t* data, std::size_t len) {
    std::vector<std::uint8_t> buf(data, data + len);
    buf.push_back(0x80u);
    while (buf.size() % 64u != 56u) {
        buf.push_back(0x00u);
    }
    std::uint8_t tail[8];
    StoreBe64(tail, static_cast<std::uint64_t>(len) * 8u);
    buf.insert(buf.end(), tail, tail + 8);
    return buf;
}

}  // namespace

Sha256Digest Sha256(const std::uint8_t* data, std::size_t len) {
    std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                          0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    const std::vector<std::uint8_t> buf = PadMessage(data, len);
    for (std::size_t off = 0; off < buf.size(); off += 64u) {
        Sha256Block(h, buf.data() + off);
    }
    Sha256Digest out{};
    for (int i = 0; i < 8; ++i) {
        StoreBe32(out.data() + 4 * i, h[i]);
    }
    return out;
}

Sha256Digest Sha256(const std::vector<std::uint8_t>& data) {
    return Sha256(data.data(), data.size());
}

Sha1Digest Sha1(const std::uint8_t* data, std::size_t len) {
    std::uint32_t h[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    const std::vector<std::uint8_t> buf = PadMessage(data, len);
    for (std::size_t off = 0; off < buf.size(); off += 64u) {
        Sha1Block(h, buf.data() + off);
    }
    Sha1Digest out{};
    for (int i = 0; i < 5; ++i) {
        StoreBe32(out.data() + 4 * i, h[i]);
    }
    return out;
}

Sha1Digest HmacSha1(const std::vector<std::uint8_t>& key,
                    const std::vector<std::uint8_t>& msg) {
    std::array<std::uint8_t, 64> k{};
    if (key.size() > k.size()) {
        const Sha1Digest kd = Sha1(key.data(), key.size());
        std::copy(kd.begin(), kd.end(), k.begin());
    } else {
        std::copy(key.begin(), key.end(), k.begin());
    }

    std::vector<std::uint8_t> inner;
    inner.reserve(k.size() + msg.size());
    for (std::uint8_t b : k) {
        inner.push_back(static_cast<std::uint8_t>(b ^ 0x36u));
    }
    inner.insert(inner.end(), msg.begin(), msg.end());
    const Sha1Digest id = Sha1(inner.data(), inner.size());

    std::vector<std::uint8_t> outer;
    outer.reserve(k.size() + id.size());
    for (std::uint8_t b : k) {
        outer.push_back(static_cast<std::uint8_t>(b ^ 0x5cu));
    }
    outer.insert(outer.end(), id.begin(), id.end());
    return Sha1(outer.data(), outer.size());
}

std::string ToHex(const std::uint8_t* data, std::size_t len) {
    static const char* kDigits = "0123456789ABCDEF";
    std::string out;
    out.reserve(len * 2u);
    for (std::size_t i = 0; i < len; ++i) {
        out.push_back(kDigits[data[i] >> 4]);
        out.push_back(kDigits[data[i] & 0x0Fu]);
    }
    return out;
}

bool FromHex(const std::string& text, std::vector<std::uint8_t>& out) {
    std::string clean;
    clean.reserve(text.size());
    for (char c : text) {
        if (std::isspace(static_cast<unsigned char>(c)) == 0) {
            clean.push_back(c);
        }
    }
    if (clean.size() % 2u != 0u) {
        return false;
    }

    out.clear();
    out.reserve(clean.size() / 2u);
    for (std::size_t i = 0; i < clean.size(); i += 2u) {
        int hi = -1;
        int lo = -1;
        for (int j = 0; j < 2; ++j) {
            const char c = clean[i + static_cast<std::size_t>(j)];
            int v = -1;
            if (c >= '0' && c <= '9') {
                v = c - '0';
            } else if (c >= 'a' && c <= 'f') {
                v = c - 'a' + 10;
            } else if (c >= 'A' && c <= 'F') {
                v = c - 'A' + 10;
            } else {
                return false;
            }
            if (j == 0) {
                hi = v;
            } else {
                lo = v;
            }
        }
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return true;
}

}  // namespace mangos::patcher
