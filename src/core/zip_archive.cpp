#include "zip_archive.hpp"
#include "platform_utils.hpp"

#include <cstring>
#include <zlib.h>

namespace omni {

namespace {

uint16_t read_u16(const uint8_t* p) {
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

uint32_t read_u32(const uint8_t* p) {
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

void write_u16(std::string& out, uint16_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
}

void write_u32(std::string& out, uint32_t v) {
    out.push_back(static_cast<char>(v & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
}

bool deflate_raw(const std::string& in, std::string& out) {
    z_stream zs{};
    if (deflateInit2(&zs, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8, Z_DEFAULT_STRATEGY) != Z_OK) {
        return false;
    }
    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(in.data()));
    zs.avail_in = static_cast<uInt>(in.size());

    out.clear();
    char buf[16384];
    int ret = Z_OK;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        ret = deflate(&zs, Z_FINISH);
        if (out.size() < zs.total_out) {
            out.append(buf, zs.total_out - out.size());
        }
    } while (ret == Z_OK);

    deflateEnd(&zs);
    return ret == Z_STREAM_END;
}

bool inflate_raw(const uint8_t* in, size_t in_size, uint32_t expected_size, std::string& out) {
    z_stream zs{};
    if (inflateInit2(&zs, -MAX_WBITS) != Z_OK) {
        return false;
    }
    zs.next_in = const_cast<Bytef*>(in);
    zs.avail_in = static_cast<uInt>(in_size);

    out.clear();
    if (expected_size > 0 && expected_size < 64u * 1024u * 1024u) {
        out.reserve(expected_size);
    }

    char buf[16384];
    int ret = Z_OK;
    do {
        zs.next_out = reinterpret_cast<Bytef*>(buf);
        zs.avail_out = sizeof(buf);
        ret = inflate(&zs, 0);
        if (out.size() < zs.total_out) {
            out.append(buf, zs.total_out - out.size());
        }
    } while (ret == Z_OK);

    inflateEnd(&zs);
    return ret == Z_STREAM_END;
}

} // namespace

bool ZipArchive::load_from_file(const std::string& path, std::string* err) {
    std::string bytes;
    if (!read_file_utf8(path, bytes, err)) {
        return false;
    }
    return load_from_memory(bytes, err);
}

bool ZipArchive::load_from_memory(const std::string& bytes, std::string* err) {
    entries_.clear();
    if (bytes.size() < 22) {
        if (err) *err = "File too small to be a valid ZIP archive";
        return false;
    }

    const uint8_t* data = reinterpret_cast<const uint8_t*>(bytes.data());
    size_t len = bytes.size();

    // Locate End of Central Directory (EOCD) signature 0x06054b50
    size_t search_start = (len > 65557) ? (len - 65557) : 0;
    size_t eocd_pos = std::string::npos;
    for (size_t i = len - 22;; --i) {
        if (data[i] == 0x50 && data[i + 1] == 0x4b && data[i + 2] == 0x05 && data[i + 3] == 0x06) {
            eocd_pos = i;
            break;
        }
        if (i == search_start) break;
    }
    if (eocd_pos == std::string::npos) {
        if (err) *err = "ZIP EOCD signature not found";
        return false;
    }

    uint16_t total_entries = read_u16(data + eocd_pos + 10);
    uint32_t cd_offset = read_u32(data + eocd_pos + 16);
    if (cd_offset >= len) {
        if (err) *err = "Invalid ZIP central directory offset";
        return false;
    }

    size_t cursor = cd_offset;
    for (uint16_t idx = 0; idx < total_entries; ++idx) {
        if (cursor + 46 > len) {
            if (err) *err = "Truncated ZIP central directory header";
            return false;
        }
        if (read_u32(data + cursor) != 0x02014b50u) {
            if (err) *err = "Invalid ZIP central directory signature";
            return false;
        }

        uint16_t method = read_u16(data + cursor + 10);
        uint32_t comp_size = read_u32(data + cursor + 20);
        uint32_t uncomp_size = read_u32(data + cursor + 24);
        uint16_t name_len = read_u16(data + cursor + 28);
        uint16_t extra_len = read_u16(data + cursor + 30);
        uint16_t comment_len = read_u16(data + cursor + 32);
        uint32_t local_offset = read_u32(data + cursor + 42);

        if (cursor + 46 + name_len > len) {
            if (err) *err = "Truncated ZIP entry name";
            return false;
        }
        std::string name(reinterpret_cast<const char*>(data + cursor + 46), name_len);
        cursor += 46 + name_len + extra_len + comment_len;

        if (local_offset + 30 > len || read_u32(data + local_offset) != 0x04034b50u) {
            if (err) *err = "Invalid local file header for " + name;
            return false;
        }
        uint16_t lf_name_len = read_u16(data + local_offset + 26);
        uint16_t lf_extra_len = read_u16(data + local_offset + 28);
        size_t payload_offset = static_cast<size_t>(local_offset) + 30 + lf_name_len + lf_extra_len;
        if (payload_offset + comp_size > len) {
            if (err) *err = "Truncated ZIP entry payload for " + name;
            return false;
        }

        if (!name.empty() && name.back() == '/') {
            continue;
        }

        std::string content;
        if (method == 0) {
            content.assign(reinterpret_cast<const char*>(data + payload_offset), comp_size);
        } else if (method == 8) {
            if (!inflate_raw(data + payload_offset, comp_size, uncomp_size, content)) {
                if (err) *err = "Failed to inflate ZIP entry: " + name;
                return false;
            }
        } else {
            if (err) *err = "Unsupported ZIP compression method for: " + name;
            return false;
        }
        entries_[std::move(name)] = std::move(content);
    }

    return true;
}

bool ZipArchive::save_to_file(const std::string& path, std::string* err) const {
    std::string bytes;
    if (!save_to_memory(bytes, err)) return false;
    return write_file_utf8(path, bytes, err);
}

bool ZipArchive::save_to_memory(std::string& out_bytes, std::string* err) const {
    out_bytes.clear();
    std::string central_dir;

    struct Meta {
        uint16_t method;
        uint32_t crc;
        uint32_t comp_size;
        uint32_t uncomp_size;
        uint32_t local_offset;
    };

    for (const auto& kv : entries_) {
        const std::string& name = kv.first;
        const std::string& raw = kv.second;

        uint32_t crc = static_cast<uint32_t>(crc32(0L, Z_NULL, 0));
        if (!raw.empty()) {
            crc = static_cast<uint32_t>(crc32(crc, reinterpret_cast<const Bytef*>(raw.data()), static_cast<uInt>(raw.size())));
        }

        std::string compressed;
        uint16_t method = 8;
        if (raw.empty() || !deflate_raw(raw, compressed) || compressed.size() >= raw.size()) {
            method = 0;
            compressed = raw;
        }

        uint32_t local_offset = static_cast<uint32_t>(out_bytes.size());
        uint16_t flags = 0x0800; // UTF-8 filename encoding
        uint16_t name_len = static_cast<uint16_t>(name.size());
        uint32_t comp_size = static_cast<uint32_t>(compressed.size());
        uint32_t uncomp_size = static_cast<uint32_t>(raw.size());

        // Local file header
        write_u32(out_bytes, 0x04034b50u);
        write_u16(out_bytes, 20); // version needed
        write_u16(out_bytes, flags);
        write_u16(out_bytes, method);
        write_u16(out_bytes, 0); // mod time
        write_u16(out_bytes, 0x5421); // mod date
        write_u32(out_bytes, crc);
        write_u32(out_bytes, comp_size);
        write_u32(out_bytes, uncomp_size);
        write_u16(out_bytes, name_len);
        write_u16(out_bytes, 0); // extra len
        out_bytes += name;
        out_bytes += compressed;

        // Central directory header
        write_u32(central_dir, 0x02014b50u);
        write_u16(central_dir, 20); // version made by
        write_u16(central_dir, 20); // version needed
        write_u16(central_dir, flags);
        write_u16(central_dir, method);
        write_u16(central_dir, 0);
        write_u16(central_dir, 0x5421);
        write_u32(central_dir, crc);
        write_u32(central_dir, comp_size);
        write_u32(central_dir, uncomp_size);
        write_u16(central_dir, name_len);
        write_u16(central_dir, 0); // extra
        write_u16(central_dir, 0); // comment
        write_u16(central_dir, 0); // disk start
        write_u16(central_dir, 0); // internal attrs
        write_u32(central_dir, 0); // external attrs
        write_u32(central_dir, local_offset);
        central_dir += name;
    }

    uint32_t cd_offset = static_cast<uint32_t>(out_bytes.size());
    uint32_t cd_size = static_cast<uint32_t>(central_dir.size());
    uint16_t count = static_cast<uint16_t>(entries_.size());

    out_bytes += central_dir;
    write_u32(out_bytes, 0x06054b50u);
    write_u16(out_bytes, 0);
    write_u16(out_bytes, 0);
    write_u16(out_bytes, count);
    write_u16(out_bytes, count);
    write_u32(out_bytes, cd_size);
    write_u32(out_bytes, cd_offset);
    write_u16(out_bytes, 0);
    (void)err;
    return true;
}

bool ZipArchive::has_entry(const std::string& name) const {
    return entries_.find(name) != entries_.end();
}

bool ZipArchive::get_entry(const std::string& name, std::string& out_data) const {
    auto it = entries_.find(name);
    if (it == entries_.end()) return false;
    out_data = it->second;
    return true;
}

void ZipArchive::set_entry(const std::string& name, std::string data) {
    entries_[name] = std::move(data);
}

std::vector<std::string> ZipArchive::list_entries() const {
    std::vector<std::string> keys;
    keys.reserve(entries_.size());
    for (const auto& kv : entries_) keys.push_back(kv.first);
    return keys;
}

} // namespace omni
