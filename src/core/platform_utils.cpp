#include "platform_utils.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <shlobj.h>
  #include <shobjidl.h>
  #include <shellapi.h>
  #include <commdlg.h>
#else
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <unistd.h>
#endif

namespace omni {

namespace fs = std::filesystem;

#ifdef _WIN32
static std::wstring utf8_to_wide(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return L"";
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
    return w;
}

static std::string wide_to_utf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
    if (n <= 0) return "";
    std::string s(static_cast<size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), static_cast<int>(w.size()), &s[0], n, nullptr, nullptr);
    return s;
}
#endif

fs::path utf8_to_path(const std::string& utf8) {
#ifdef _WIN32
    return fs::path(utf8_to_wide(utf8));
#else
    return fs::path(utf8);
#endif
}

std::string path_to_utf8(const fs::path& p) {
#ifdef _WIN32
    return wide_to_utf8(p.wstring());
#else
    return p.string();
#endif
}

bool read_file_utf8(const std::string& path, std::string& out_content, std::string* err) {
    std::ifstream ifs(utf8_to_path(path), std::ios::binary | std::ios::ate);
    if (!ifs) {
        if (err) *err = "Cannot open file for reading: " + path;
        return false;
    }
    std::streamsize sz = ifs.tellg();
    ifs.seekg(0, std::ios::beg);
    out_content.assign(static_cast<size_t>(sz), '\0');
    if (sz > 0 && !ifs.read(&out_content[0], sz)) {
        if (err) *err = "Failed reading file: " + path;
        return false;
    }
    return true;
}

bool write_file_utf8(const std::string& path, const std::string& content, std::string* err) {
    std::error_code ec;
    fs::path p = utf8_to_path(path);
    if (p.has_parent_path()) {
        fs::create_directories(p.parent_path(), ec);
    }
    std::ofstream ofs(p, std::ios::binary);
    if (!ofs) {
        if (err) *err = "Cannot open file for writing: " + path;
        return false;
    }
    ofs.write(content.data(), static_cast<std::streamsize>(content.size()));
    if (!ofs) {
        if (err) *err = "Failed writing file: " + path;
        return false;
    }
    return true;
}

std::string get_app_data_dir() {
#ifdef _WIN32
    const char* appdata = std::getenv("LOCALAPPDATA");
    if (!appdata) appdata = std::getenv("APPDATA");
    fs::path base = appdata ? fs::path(appdata) : fs::current_path();
    fs::path dir = base / "OmniWorkbench";
#else
    const char* home = std::getenv("HOME");
    fs::path base = home ? fs::path(home) : fs::current_path();
    fs::path dir = base / ".config" / "OmniWorkbench";
#endif
    std::error_code ec;
    fs::create_directories(dir, ec);
    return path_to_utf8(dir);
}

std::string get_temp_dir() {
    std::error_code ec;
    fs::path tmp = fs::temp_directory_path(ec);
    if (ec || tmp.empty()) return ".";
    return path_to_utf8(tmp);
}

std::string get_default_vault_dir() {
#ifdef _WIN32
    const char* up = std::getenv("USERPROFILE");
    fs::path base = up ? fs::path(up) : fs::current_path();
    fs::path dir = base / "Documents" / "OmniVault";
#else
    const char* home = std::getenv("HOME");
    fs::path base = home ? fs::path(home) : fs::current_path();
    fs::path dir = base / "OmniVault";
#endif
    return path_to_utf8(dir);
}

std::string pick_folder_dialog(const std::string& title, const std::string& initial_dir) {
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    bool should_uninit = SUCCEEDED(hr);
    std::string result;

    IFileOpenDialog* pfd = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&pfd)))) {
        DWORD opts = 0;
        pfd->GetOptions(&opts);
        pfd->SetOptions(opts | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST | FOS_FORCEFILESYSTEM);
        std::wstring wtitle = utf8_to_wide(title);
        pfd->SetTitle(wtitle.c_str());

        if (!initial_dir.empty()) {
            IShellItem* init_item = nullptr;
            std::wstring winit = utf8_to_wide(initial_dir);
            if (SUCCEEDED(SHCreateItemFromParsingName(winit.c_str(), nullptr, IID_PPV_ARGS(&init_item)))) {
                pfd->SetFolder(init_item);
                init_item->Release();
            }
        }

        if (SUCCEEDED(pfd->Show(nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(pfd->GetResult(&item))) {
                PWSTR pszPath = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &pszPath)) && pszPath) {
                    result = wide_to_utf8(pszPath);
                    CoTaskMemFree(pszPath);
                }
                item->Release();
            }
        }
        pfd->Release();
    }
    if (should_uninit) CoUninitialize();
    return result;
#else
    (void)title;
    (void)initial_dir;
    // Try zenity or kdialog on Linux if available
    FILE* pipe = popen("command -v zenity >/dev/null 2>&1 && zenity --file-selection --directory --title='选择知识库目录' 2>/dev/null", "r");
    if (!pipe) return "";
    char buf[1024];
    std::string out;
    while (fgets(buf, sizeof(buf), pipe)) out += buf;
    pclose(pipe);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
#endif
}

std::string open_file_dialog(const std::string& title, const std::string& filter_desc, const std::string& filter_pattern) {
#ifdef _WIN32
    wchar_t file_buf[MAX_PATH] = {0};
    std::wstring wtitle = utf8_to_wide(title);
    std::wstring wdesc = utf8_to_wide(filter_desc);
    std::wstring wpat = utf8_to_wide(filter_pattern);

    std::wstring filter = wdesc + L'\0' + wpat + L'\0' + L"All Files (*.*)" + L'\0' + L"*.*" + L'\0';
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wtitle.c_str();
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        return wide_to_utf8(file_buf);
    }
    return "";
#else
    (void)title;
    (void)filter_desc;
    (void)filter_pattern;
    return "";
#endif
}

std::string save_file_dialog(const std::string& title, const std::string& default_name, const std::string& filter_ext) {
#ifdef _WIN32
    wchar_t file_buf[MAX_PATH] = {0};
    std::wstring wdef = utf8_to_wide(default_name);
    if (wdef.size() < MAX_PATH) {
        wdef.copy(file_buf, wdef.size());
    }
    std::wstring wtitle = utf8_to_wide(title);
    std::wstring wext = utf8_to_wide(filter_ext);

    std::wstring filter = L"Target (*." + wext + L")\0*." + wext + L"\0All Files (*.*)\0*.*\0";

    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = filter.c_str();
    ofn.lpstrFile = file_buf;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = wtitle.c_str();
    ofn.lpstrDefExt = wext.c_str();
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;

    if (GetSaveFileNameW(&ofn)) {
        return wide_to_utf8(file_buf);
    }
    return "";
#else
    (void)title;
    (void)default_name;
    (void)filter_ext;
    return "";
#endif
}

bool open_in_file_manager(const std::string& path) {
#ifdef _WIN32
    std::wstring wpath = utf8_to_wide(path);
    HINSTANCE res = ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return reinterpret_cast<intptr_t>(res) > 32;
#else
    std::string cmd = "xdg-open \"" + path + "\" >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
#endif
}

std::string find_chromium_or_edge() {
#ifdef _WIN32
    const char* candidates[] = {
        "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
        "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
        "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
        "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe"
    };
#else
    const char* candidates[] = {
        "/usr/bin/google-chrome",
        "/usr/bin/chromium",
        "/usr/bin/chromium-browser",
        "/usr/bin/microsoft-edge",
        "/mnt/c/Program Files (x86)/Microsoft/Edge/Application/msedge.exe",
        "/mnt/c/Program Files/Microsoft/Edge/Application/msedge.exe"
    };
#endif
    std::error_code ec;
    for (const char* p : candidates) {
        if (fs::exists(utf8_to_path(p), ec)) {
            return p;
        }
    }
    return "";
}

std::string to_win_path_if_wsl(const std::string& path) {
#ifdef _WIN32
    return path;
#else
    if (path.rfind("/mnt/", 0) == 0 && path.size() >= 7 && std::isalpha(static_cast<unsigned char>(path[5])) && path[6] == '/') {
        char drive = static_cast<char>(std::toupper(static_cast<unsigned char>(path[5])));
        std::string rest = path.substr(6);
        for (char& c : rest) {
            if (c == '/') c = '\\';
        }
        return std::string(1, drive) + ":" + rest;
    }
    return path;
#endif
}

int run_command_silent(const std::string& exe_path, const std::vector<std::string>& args) {
#ifdef _WIN32
    std::ostringstream cmd;
    cmd << "\"" << exe_path << "\"";
    for (const auto& a : args) {
        cmd << " \"" << a << "\"";
    }
    std::wstring wcmd = utf8_to_wide(cmd.str());
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &wcmd[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        return -1;
    }
    WaitForSingleObject(pi.hProcess, 30000);
    DWORD exit_code = 1;
    GetExitCodeProcess(pi.hProcess, &exit_code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return static_cast<int>(exit_code);
#else
    pid_t pid = fork();
    if (pid == 0) {
        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe_path.c_str()));
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(exe_path.c_str(), argv.data());
        _exit(127);
    }
    if (pid < 0) return -1;
    int status = 0;
    waitpid(pid, &status, 0);
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    return -1;
#endif
}

} // namespace omni
