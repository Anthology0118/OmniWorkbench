#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace omni {

/**
 * Pure C++17 + zlib PKZIP archive reader/writer for OpenXML (.docx) packages.
 */
class ZipArchive {
public:
    struct Entry {
        std::string name;
        std::string data;
    };

    bool load_from_file(const std::string& path, std::string* err = nullptr);
    bool load_from_memory(const std::string& bytes, std::string* err = nullptr);
    bool save_to_file(const std::string& path, std::string* err = nullptr) const;
    bool save_to_memory(std::string& out_bytes, std::string* err = nullptr) const;

    bool has_entry(const std::string& name) const;
    bool get_entry(const std::string& name, std::string& out_data) const;
    void set_entry(const std::string& name, std::string data);
    std::vector<std::string> list_entries() const;

private:
    std::map<std::string, std::string> entries_;
};

} // namespace omni
