#pragma once

#include <set>
#include <string>
#include <vector>

namespace omni {

struct AppConfig {
    std::string theme = "white";       // "white" | "brown" | "dark"
    std::string active_tab = "english"; // "english" | "knowledge" | "tools"
    std::string vault_path;
    std::set<std::string> marked_words;
    std::set<std::string> favorite_words;
    std::vector<std::string> recent_docs;
};

class ConfigManager {
public:
    ConfigManager();

    const AppConfig& get() const { return cfg_; }
    AppConfig& mutable_get() { return cfg_; }

    bool load();
    bool save() const;
    std::string config_file_path() const { return config_path_; }

private:
    std::string config_path_;
    AppConfig cfg_;
};

} // namespace omni
