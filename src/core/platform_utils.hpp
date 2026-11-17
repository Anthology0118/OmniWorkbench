#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace omni {

/**
 * Create a std::filesystem::path cleanly from a UTF-8 std::string (crucial on Windows).
 */
std::filesystem::path utf8_to_path(const std::string& utf8);
std::string path_to_utf8(const std::filesystem::path& p);

bool read_file_utf8(const std::string& path, std::string& out_content, std::string* err = nullptr);
bool write_file_utf8(const std::string& path, const std::string& content, std::string* err = nullptr);

std::string get_app_data_dir();
std::string get_temp_dir();
std::string get_default_vault_dir();

/**
 * Open native OS folder picker dialog (Windows IFileOpenDialog / fallback).
 * Returns empty string if cancelled or unsupported headless.
 */
std::string pick_folder_dialog(const std::string& title, const std::string& initial_dir = "");

/**
 * Open native OS open/save file dialog.
 */
std::string open_file_dialog(const std::string& title, const std::string& filter_desc = "Dictionary Files (*.json;*.csv)", const std::string& filter_pattern = "*.json;*.csv");
std::string save_file_dialog(const std::string& title, const std::string& default_name, const std::string& filter_ext);

/**
 * Reveal / open folder in OS file manager.
 */
bool open_in_file_manager(const std::string& path);

/**
 * Locate headless Edge/Chrome binary if present on host.
 */
std::string find_chromium_or_edge();

/**
 * Run a subprocess silently (without flashing a console window on Windows).
 */
int run_command_silent(const std::string& exe_path, const std::vector<std::string>& args);

/**
 * Convert a WSL path (/mnt/c/...) to Windows path (C:\...) when invoking Windows .exe from WSL2.
 */
std::string to_win_path_if_wsl(const std::string& path);

} // namespace omni
