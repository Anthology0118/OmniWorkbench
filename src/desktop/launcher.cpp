#ifndef NOMINMAX
  #define NOMINMAX
#endif
#include <windows.h>

#include <string>
#include <vector>

namespace {

bool file_exists(const std::wstring& path) {
    DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY);
}

std::wstring dir_of(const std::wstring& path) {
    size_t pos = path.find_last_of(L"\\/");
    return pos == std::wstring::npos ? L"." : path.substr(0, pos);
}

std::wstring get_local_appdata_qt() {
    wchar_t buf[MAX_PATH] = {};
    DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) return L"";
    return std::wstring(buf) + L"\\OmniWorkbench\\qt_app\\OmniWorkbenchQt.exe";
}

} // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR cmdLine, int) {
    wchar_t exePath[MAX_PATH] = {};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    std::wstring exeDir = dir_of(exePath);

    std::vector<std::wstring> candidates = {
        exeDir + L"\\qt_app\\OmniWorkbenchQt.exe",
        exeDir + L"\\OmniWorkbenchQt.exe",
        get_local_appdata_qt()
    };

    std::wstring target;
    for (const auto& c : candidates) {
        if (!c.empty() && file_exists(c)) {
            target = c;
            break;
        }
    }

    if (target.empty()) {
        MessageBoxW(nullptr,
            L"未找到 Qt 6 运行组件 OmniWorkbenchQt.exe。\n"
            L"请解压 OmniWorkbench-win64.zip 压缩包并保留其中的 qt_app 文件夹后重试。",
            L"OmniWorkbench 启动提示", MB_OK | MB_ICONWARNING);
        return 1;
    }

    std::wstring targetDir = dir_of(target);
    std::wstring fullCmd = L"\"" + target + L"\"";
    if (cmdLine && cmdLine[0] != L'\0') {
        fullCmd += L" ";
        fullCmd += cmdLine;
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, &fullCmd[0], nullptr, nullptr, FALSE, 0,
                        nullptr, targetDir.c_str(), &si, &pi)) {
        MessageBoxW(nullptr, L"启动 OmniWorkbenchQt.exe 失败，请检查杀毒软件拦截或重新解压。",
                    L"OmniWorkbench", MB_OK | MB_ICONERROR);
        return 1;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return 0;
}
