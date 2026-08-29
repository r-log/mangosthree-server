#include "ClientFile.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace mangos::patcher {

bool ClientFile::Load(const std::string& path, ClientFile& out, std::string& error) {
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in) {
        error = "cannot open " + path;
        return false;
    }
    const std::streamsize len = in.tellg();
    if (len <= 0) {
        error = "empty file: " + path;
        return false;
    }
    in.seekg(0, std::ios::beg);

    out.bytes_.resize(static_cast<std::size_t>(len));
    if (!in.read(reinterpret_cast<char*>(out.bytes_.data()), len)) {
        error = "short read on " + path;
        return false;
    }

    std::string pe_error;
    const std::optional<PeImage> pe = PeImage::Parse(out.bytes_, pe_error);
    if (!pe.has_value()) {
        error = path + ": " + pe_error;
        return false;
    }
    out.pe_ = *pe;
    return true;
}

bool ClientFile::Save(const std::string& path, bool backup, std::string& error) const {
    if (backup) {
        const std::filesystem::path src(path);
        const std::filesystem::path dst(path + ".bak");
        std::error_code ec;
        if (!std::filesystem::exists(dst, ec)) {
            std::filesystem::copy_file(src, dst, ec);
            if (ec) {
                error = "cannot write backup " + dst.string() + ": " + ec.message();
                return false;
            }
        }
    }

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f) {
        error = "cannot open for writing: " + path;
        return false;
    }
    f.write(reinterpret_cast<const char*>(bytes_.data()),
            static_cast<std::streamsize>(bytes_.size()));
    if (!f) {
        error = "write failed: " + path;
        return false;
    }
    return true;
}

std::vector<std::size_t> ClientFile::FindAll(const std::vector<std::uint8_t>& pattern) const {
    std::vector<std::size_t> hits;
    if (pattern.empty() || pattern.size() > bytes_.size()) {
        return hits;
    }
    auto it = bytes_.begin();
    while (true) {
        it = std::search(it, bytes_.end(), pattern.begin(), pattern.end());
        if (it == bytes_.end()) {
            return hits;
        }
        hits.push_back(static_cast<std::size_t>(it - bytes_.begin()));
        ++it;
    }
}

bool ClientFile::ReadAtVa(std::uint64_t va, std::size_t len,
                          std::vector<std::uint8_t>& out) const {
    const std::optional<std::size_t> off = pe_.FileOffsetOf(va);
    if (!off.has_value() || *off + len > bytes_.size()) {
        return false;
    }
    out.assign(bytes_.begin() + static_cast<std::ptrdiff_t>(*off),
               bytes_.begin() + static_cast<std::ptrdiff_t>(*off + len));
    return true;
}

bool ClientFile::WriteAtVa(std::uint64_t va, const std::vector<std::uint8_t>& data,
                           std::string& error) {
    const std::optional<std::size_t> off = pe_.FileOffsetOf(va);
    if (!off.has_value() || *off + data.size() > bytes_.size()) {
        error = "virtual address is not backed by file bytes";
        return false;
    }
    WriteAtOffset(*off, data);
    return true;
}

void ClientFile::WriteAtOffset(std::size_t offset, const std::vector<std::uint8_t>& data) {
    std::copy(data.begin(), data.end(),
              bytes_.begin() + static_cast<std::ptrdiff_t>(offset));
}

}  // namespace mangos::patcher
