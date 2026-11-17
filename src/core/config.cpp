#include "config.hpp"

#include "json.hpp"
#include "platform_utils.hpp"

#include <filesystem>

namespace omni {

namespace fs = std::filesystem;

ConfigManager::ConfigManager() {
    fs::path p = utf8_to_path(get_app_data_dir()) / "omni_config.json";
    config_path_ = path_to_utf8(p);
    cfg_.vault_path = "";
    cfg_.marked_words.insert("insight");
    load();
}

bool ConfigManager::load() {
    std::string raw;
    if (!read_file_utf8(config_path_, raw)) {
        return false;
    }
    JsonValue root;
    if (!JsonValue::parse(raw, root) || !root.is_object()) {
        return false;
    }
    if (root.has("theme")) cfg_.theme = root.get("theme").as_string("white");
    if (root.has("active_tab")) cfg_.active_tab = root.get("active_tab").as_string("english");
    if (root.has("vault_path")) {
        cfg_.vault_path = root.get("vault_path").as_string("");
    }
    if (root.get("marked_words").is_array()) {
        cfg_.marked_words.clear();
        for (const auto& item : root.get("marked_words").arr_val) {
            if (item.is_string()) cfg_.marked_words.insert(item.as_string());
        }
    }
    if (root.get("favorite_words").is_array()) {
        cfg_.favorite_words.clear();
        for (const auto& item : root.get("favorite_words").arr_val) {
            if (item.is_string()) cfg_.favorite_words.insert(item.as_string());
        }
    }
    if (root.get("recent_docs").is_array()) {
        cfg_.recent_docs.clear();
        for (const auto& item : root.get("recent_docs").arr_val) {
            if (item.is_string()) cfg_.recent_docs.push_back(item.as_string());
        }
    }
    return true;
}

bool ConfigManager::save() const {
    JsonValue root = JsonValue::make_object();
    root["theme"] = cfg_.theme;
    root["active_tab"] = cfg_.active_tab;
    root["vault_path"] = cfg_.vault_path;

    JsonValue marked = JsonValue::make_array();
    for (const auto& w : cfg_.marked_words) marked.push_back(w);
    root["marked_words"] = std::move(marked);

    JsonValue favs = JsonValue::make_array();
    for (const auto& w : cfg_.favorite_words) favs.push_back(w);
    root["favorite_words"] = std::move(favs);

    JsonValue recent = JsonValue::make_array();
    for (const auto& r : cfg_.recent_docs) recent.push_back(r);
    root["recent_docs"] = std::move(recent);

    return write_file_utf8(config_path_, root.dump(2));
}

} // namespace omni
