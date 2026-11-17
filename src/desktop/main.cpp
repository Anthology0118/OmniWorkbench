#include "../core/docx_converter.hpp"
#include "../core/pdf_converter.hpp"
#include "../core/platform_utils.hpp"
#include "../generated/embedded_ui.hpp"
#include "../server/app_server.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
  #include <shellapi.h>
#endif

namespace {

std::string lower_ext(const std::string& path) {
    std::string ext = omni::path_to_utf8(omni::utf8_to_path(path).extension());
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

int run_cli_convert(const std::string& input_path, const std::string& to_fmt, const std::string& out_path) {
    std::string ext = lower_ext(input_path);
    std::string err;
    std::string final_out = out_path;
    if (final_out.empty()) {
        std::filesystem::path p = omni::utf8_to_path(input_path);
        final_out = omni::path_to_utf8(p.parent_path() / (omni::path_to_utf8(p.stem()) + "." + to_fmt));
    }

    bool ok = false;
    if (to_fmt == "docx") {
        std::string md;
        if (ext == ".docx") {
            ok = omni::docx_to_markdown(input_path, md, &err) && omni::markdown_to_docx(md, final_out, &err);
        } else {
            ok = omni::read_file_utf8(input_path, md, &err) && omni::markdown_to_docx(md, final_out, &err);
        }
    } else if (to_fmt == "md") {
        if (ext == ".docx") {
            std::string md;
            ok = omni::docx_to_markdown(input_path, md, &err) && omni::write_file_utf8(final_out, md, &err);
        } else {
            std::string md;
            ok = omni::read_file_utf8(input_path, md, &err) && omni::write_file_utf8(final_out, md, &err);
        }
    } else if (to_fmt == "pdf") {
        if (ext == ".docx") {
            ok = omni::docx_to_pdf(input_path, final_out, &err);
        } else {
            std::string md;
            std::string title = omni::path_to_utf8(omni::utf8_to_path(input_path).stem());
            ok = omni::read_file_utf8(input_path, md, &err) && omni::markdown_to_pdf(md, final_out, title, &err);
        }
    } else {
        std::cerr << "Unsupported format: " << to_fmt << "\n";
        return 1;
    }

    if (!ok) {
        std::cerr << "Conversion failed: " << err << "\n";
        return 1;
    }
    std::cout << "Converted successfully -> " << final_out << "\n";
    return 0;
}

bool launch_desktop_window(uint16_t port) {
    std::string url = "http://127.0.0.1:" + std::to_string(port);
#ifdef _WIN32
    std::string browser = omni::find_chromium_or_edge();
    if (!browser.empty()) {
        std::string profile_dir = omni::get_app_data_dir() + "\\WebProfile";
        std::string cmd = "\"" + browser + "\" --app=" + url +
                          " --window-size=1380,860 --user-data-dir=\"" + profile_dir + "\"";
        int n = MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, nullptr, 0);
        std::wstring wcmd(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, cmd.c_str(), -1, &wcmd[0], n);

        STARTUPINFOW si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        if (CreateProcessW(nullptr, &wcmd[0], nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
    std::wstring wurl(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, &wurl[0], n);
    ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    return true;
#else
    std::string browser = omni::find_chromium_or_edge();
    if (!browser.empty()) {
        std::string cmd = "\"" + browser + "\" --app=" + url + " --window-size=1380,860 >/dev/null 2>&1 &";
        return std::system(cmd.c_str()) == 0;
    }
    std::string cmd = "xdg-open \"" + url + "\" >/dev/null 2>&1 &";
    return std::system(cmd.c_str()) == 0;
#endif
}

} // namespace

int main(int argc, char** argv) {
    bool open_window = true;
    uint16_t port = 0;
    std::string conv_in;
    std::string conv_to;
    std::string conv_out;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--no-browser" || arg == "--headless") {
            open_window = false;
        } else if (arg == "--port" && i + 1 < argc) {
            port = static_cast<uint16_t>(std::stoi(argv[++i]));
        } else if (arg == "--convert" && i + 1 < argc) {
            conv_in = argv[++i];
        } else if (arg == "--to" && i + 1 < argc) {
            conv_to = argv[++i];
        } else if ((arg == "-o" || arg == "--out") && i + 1 < argc) {
            conv_out = argv[++i];
        }
    }

    if (!conv_in.empty() && !conv_to.empty()) {
        return run_cli_convert(conv_in, conv_to, conv_out);
    }

    omni::AppServer server(omni::get_embedded_ui_html());
    if (!server.start(port)) {
        std::cerr << "Failed to bind localhost HTTP server.\n";
        return 1;
    }

    std::cout << "OmniWorkbench running at http://127.0.0.1:" << server.port() << std::endl;
    if (open_window) {
        server.set_auto_exit_on_close(true);
        launch_desktop_window(server.port());
    }

    server.wait_until_stopped();
    return 0;
}

#ifdef _WIN32
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return main(__argc, __argv);
}
#endif
