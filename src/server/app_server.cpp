#include "app_server.hpp"

#include "../core/docx_converter.hpp"
#include "../core/json.hpp"
#include "../core/markdown.hpp"
#include "../core/pdf_converter.hpp"
#include "../core/platform_utils.hpp"
#include "../generated/embedded_ui.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  using socklen_t = int;
#else
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <poll.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

namespace omni {

namespace fs = std::filesystem;

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

void close_socket_fd(uintptr_t s) {
#ifdef _WIN32
    if (s != INVALID_SOCKET) closesocket(static_cast<SOCKET>(s));
#else
    int fd = static_cast<int>(s);
    if (fd >= 0) ::close(fd);
#endif
}

bool send_all(uintptr_t sock, const char* data, size_t len) {
    size_t sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int n = ::send(static_cast<SOCKET>(sock), data + sent, static_cast<int>(len - sent), 0);
#else
        ssize_t n = ::send(static_cast<int>(sock), data + sent, len - sent, 0);
#endif
        if (n <= 0) return false;
        sent += static_cast<size_t>(n);
    }
    return true;
}

std::string status_text(int code) {
    switch (code) {
        case 200: return "OK";
        case 400: return "Bad Request";
        case 404: return "Not Found";
        case 500: return "Internal Server Error";
        default: return "OK";
    }
}

JsonValue make_error(const std::string& msg) {
    JsonValue obj = JsonValue::make_object();
    obj["ok"] = false;
    obj["error"] = msg;
    return obj;
}

std::string decode_b64_bytes(const std::string& in) {
    static int T[256];
    static bool init = false;
    if (!init) {
        std::fill(std::begin(T), std::end(T), -1);
        const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) T[static_cast<unsigned char>(tbl[i])] = i;
        init = true;
    }
    std::string out;
    int val = 0, valb = -8;
    for (unsigned char c : in) {
        if (T[c] == -1) continue;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            out.push_back(static_cast<char>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    return out;
}

DocxExportContext build_docx_ctx(const JsonValue& body) {
    DocxExportContext ctx;
    if (body.has("diagram_pngs") && body.get("diagram_pngs").is_object()) {
        for (const auto& kv : body.get("diagram_pngs").obj_val) {
            if (kv.second.is_string()) {
                ctx.diagram_pngs[kv.first] = kv.second.as_string();
            }
        }
    }
    return ctx;
}

} // namespace

AppServer::AppServer(std::string ui_html)
    : ui_html_(std::move(ui_html)),
      vault_(config_.get().vault_path) {
    dict_.init();
}

AppServer::~AppServer() {
    stop();
}

bool AppServer::start(uint16_t preferred_port) {
#ifdef _WIN32
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return false;
    }
    SOCKET s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) return false;
#else
    int s = ::socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) return false;
#endif

    int opt = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(preferred_port);

    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        close_socket_fd(static_cast<uintptr_t>(s));
        return false;
    }

    socklen_t alen = sizeof(addr);
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&addr), &alen) != 0) {
        close_socket_fd(static_cast<uintptr_t>(s));
        return false;
    }
    port_ = ntohs(addr.sin_port);

    if (::listen(s, 64) != 0) {
        close_socket_fd(static_cast<uintptr_t>(s));
        return false;
    }

    listen_sock_ = static_cast<uintptr_t>(s);
    running_.store(true);
    shutdown_requested_.store(false);
    last_ping_ms_.store(now_ms());

    accept_thread_ = std::thread(&AppServer::accept_loop, this);
    return true;
}

void AppServer::stop() {
    bool expected = true;
    if (running_.compare_exchange_strong(expected, false)) {
        shutdown_requested_.store(true);
#ifdef _WIN32
        if (listen_sock_ != INVALID_SOCKET) {
            ::shutdown(static_cast<SOCKET>(listen_sock_), SD_BOTH);
            close_socket_fd(listen_sock_);
            listen_sock_ = INVALID_SOCKET;
        }
#else
        if (listen_sock_ >= 0) {
            ::shutdown(listen_sock_, SHUT_RDWR);
            close_socket_fd(static_cast<uintptr_t>(listen_sock_));
            listen_sock_ = -1;
        }
#endif
        if (accept_thread_.joinable()) {
            accept_thread_.join();
        }
#ifdef _WIN32
        WSACleanup();
#endif
    }
}

void AppServer::wait_until_stopped() {
    while (running_.load() && !shutdown_requested_.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        if (auto_exit_on_close_ && has_seen_client_.load()) {
            if (now_ms() - last_ping_ms_.load() > 6000) {
                shutdown_requested_.store(true);
                break;
            }
        }
    }
    stop();
}

void AppServer::accept_loop() {
    while (running_.load()) {
#ifdef _WIN32
        fd_set rfds;
        FD_ZERO(&rfds);
        SOCKET ls = static_cast<SOCKET>(listen_sock_);
        if (ls == INVALID_SOCKET) break;
        FD_SET(ls, &rfds);
        timeval tv{0, 150000};
        int ready = ::select(0, &rfds, nullptr, nullptr, &tv);
        if (ready <= 0) continue;
        sockaddr_in cli{};
        socklen_t clen = sizeof(cli);
        SOCKET c = ::accept(ls, reinterpret_cast<sockaddr*>(&cli), &clen);
        if (c == INVALID_SOCKET) break;
        uintptr_t fd = static_cast<uintptr_t>(c);
#else
        pollfd pfd{};
        pfd.fd = listen_sock_;
        pfd.events = POLLIN;
        int ready = ::poll(&pfd, 1, 150);
        if (ready < 0) break;
        if (ready == 0) continue;
        sockaddr_in cli{};
        socklen_t clen = sizeof(cli);
        int c = ::accept(listen_sock_, reinterpret_cast<sockaddr*>(&cli), &clen);
        if (c < 0) break;
        uintptr_t fd = static_cast<uintptr_t>(c);
#endif
        std::thread([this, fd]() {
            this->handle_client(fd);
        }).detach();
    }
}

void AppServer::handle_client(uintptr_t client_sock) {
    std::string raw;
    char buf[4096];
    size_t header_end = std::string::npos;
    size_t content_length = 0;

    while (true) {
#ifdef _WIN32
        int n = ::recv(static_cast<SOCKET>(client_sock), buf, sizeof(buf), 0);
#else
        ssize_t n = ::recv(static_cast<int>(client_sock), buf, sizeof(buf), 0);
#endif
        if (n <= 0) break;
        raw.append(buf, static_cast<size_t>(n));

        if (header_end == std::string::npos) {
            header_end = raw.find("\r\n\r\n");
            if (header_end != std::string::npos) {
                std::string headers = raw.substr(0, header_end);
                std::string lower_h = headers;
                for (char& ch : lower_h) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
                size_t cl_pos = lower_h.find("content-length:");
                if (cl_pos != std::string::npos) {
                    size_t line_end = lower_h.find("\r\n", cl_pos);
                    std::string val = headers.substr(cl_pos + 15, (line_end == std::string::npos ? headers.size() : line_end) - (cl_pos + 15));
                    try { content_length = static_cast<size_t>(std::stoul(val)); } catch (...) { content_length = 0; }
                }
            }
        }
        if (header_end != std::string::npos) {
            if (raw.size() >= header_end + 4 + content_length) {
                break;
            }
        }
        if (raw.size() > 32 * 1024 * 1024) break;
    }

    if (header_end != std::string::npos) {
        std::string header_block = raw.substr(0, header_end);
        size_t first_line_end = header_block.find("\r\n");
        std::string req_line = header_block.substr(0, first_line_end);

        std::istringstream iss(req_line);
        std::string method, target;
        iss >> method >> target;

        HttpRequest req;
        req.method = method;
        size_t qm = target.find('?');
        if (qm == std::string::npos) {
            req.path = target;
        } else {
            req.path = target.substr(0, qm);
            req.query = target.substr(qm + 1);
        }
        if (content_length > 0 && raw.size() >= header_end + 4 + content_length) {
            req.body = raw.substr(header_end + 4, content_length);
        }

        HttpResponse resp = route_request(req);
        std::ostringstream out;
        out << "HTTP/1.1 " << resp.status << " " << status_text(resp.status) << "\r\n"
            << "Content-Type: " << resp.content_type << "\r\n"
            << "Content-Length: " << resp.body.size() << "\r\n"
            << "Cache-Control: no-store\r\n"
            << "Connection: close\r\n\r\n";
        std::string hdr = out.str();
        send_all(client_sock, hdr.data(), hdr.size());
        if (!resp.body.empty()) {
            send_all(client_sock, resp.body.data(), resp.body.size());
        }
    }

    close_socket_fd(client_sock);
}

std::string AppServer::url_decode(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '+') {
            out.push_back(' ');
        } else if (s[i] == '%' && i + 2 < s.size()) {
            try {
                int v = std::stoi(s.substr(i + 1, 2), nullptr, 16);
                out.push_back(static_cast<char>(v));
                i += 2;
            } catch (...) {
                out.push_back(s[i]);
            }
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

std::string AppServer::get_query_param(const std::string& query, const std::string& key) {
    std::istringstream iss(query);
    std::string part;
    while (std::getline(iss, part, '&')) {
        size_t eq = part.find('=');
        if (eq != std::string::npos) {
            if (url_decode(part.substr(0, eq)) == key) {
                return url_decode(part.substr(eq + 1));
            }
        }
    }
    return "";
}

AppServer::HttpResponse AppServer::route_request(const HttpRequest& req) {
    if (req.method == "GET" && (req.path == "/" || req.path == "/index.html")) {
        has_seen_client_.store(true);
        last_ping_ms_.store(now_ms());
        return {200, "text/html; charset=utf-8", ui_html_};
    }

    if (req.method == "GET" && (req.path == "/app_icon.png" || req.path == "/favicon.ico")) {
        return {200, "image/png", get_embedded_icon_png()};
    }

    if (req.path == "/api/window-ping") {
        has_seen_client_.store(true);
        last_ping_ms_.store(now_ms());
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        std::unique_lock<std::mutex> try_lk(state_mutex_, std::try_to_lock);
        if (try_lk.owns_lock()) {
            res["vault_path"] = vault_.root();
            res["root_exists"] = vault_.root_exists();
            res["vault_sig"] = vault_.compute_signature();
        }
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/shutdown") {
        shutdown_requested_.store(true);
        return {200, "application/json; charset=utf-8", "{\"ok\":true}"};
    }

    std::lock_guard<std::mutex> lock(state_mutex_);

    if (req.method == "GET" && req.path == "/api/state") {
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["theme"] = config_.get().theme;
        res["active_tab"] = config_.get().active_tab;
        res["vault_path"] = vault_.root();
        res["tree"] = vault_.scan_tree();
        res["dict_total"] = dict_.total_count();

        JsonValue words = JsonValue::make_array();
        for (const auto& w : dict_.search("", 30)) {
            words.push_back(dict_.entry_to_json(w));
        }
        res["words"] = std::move(words);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/config") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        if (body.has("theme")) config_.mutable_get().theme = body.get("theme").as_string("white");
        if (body.has("active_tab")) config_.mutable_get().active_tab = body.get("active_tab").as_string("english");
        if (body.has("vault_path")) {
            std::string vp = body.get("vault_path").as_string();
            config_.mutable_get().vault_path = vp;
            vault_.set_root(vp);
            if (!vp.empty()) {
                vault_.ensure_initialized(false);
            }
        }
        config_.save();
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["vault_path"] = vault_.root();
        res["tree"] = vault_.scan_tree();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/vault/pick") {
        std::string picked = pick_folder_dialog("选择本地知识库文件夹", vault_.root());
        if (picked.empty()) {
            JsonValue res = JsonValue::make_object();
            res["ok"] = false;
            res["cancelled"] = true;
            return {200, "application/json; charset=utf-8", res.dump()};
        }
        config_.mutable_get().vault_path = picked;
        config_.save();
        vault_.set_root(picked);
        vault_.ensure_initialized(false);

        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["vault_path"] = picked;
        res["tree"] = vault_.scan_tree();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/vault/watch") {
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["vault_path"] = vault_.root();
        res["root_exists"] = vault_.root_exists();
        res["vault_sig"] = vault_.compute_signature();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/vault/tree") {
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["tree"] = vault_.scan_tree();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/vault/file") {
        std::string p = get_query_param(req.query, "path");
        VaultFileContent fc;
        std::string err;
        if (!vault_.open_document(p, fc, &err)) {
            return {404, "application/json; charset=utf-8", make_error(err).dump()};
        }
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["rel_path"] = fc.rel_path;
        res["abs_path"] = fc.abs_path;
        res["ext"] = fc.ext;
        res["is_docx"] = fc.is_docx;
        res["markdown"] = fc.markdown;
        res["html"] = fc.html;
        JsonValue formulas = JsonValue::make_array();
        for (const auto& f : extract_latex_blocks(fc.markdown)) formulas.push_back(f);
        res["formulas"] = std::move(formulas);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/vault/file") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string path = body.get("path").as_string();
        std::string md = body.get("markdown").as_string();
        DocxExportContext dctx = build_docx_ctx(body);
        std::string err;
        if (!vault_.save_document(path, md, &err, &dctx)) {
            return {500, "application/json; charset=utf-8", make_error(err).dump()};
        }
        MdDocument doc = parse_markdown(md);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["html"] = render_markdown_to_html(doc, false, "OmniWorkbench Document", path);
        res["tree"] = vault_.scan_tree();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/vault/asset") {
        std::string doc_rel = get_query_param(req.query, "doc");
        std::string asset_rel = get_query_param(req.query, "path");
        if (asset_rel.empty()) return {400, "text/plain", "missing path"};
        fs::path root_p = fs::weakly_canonical(utf8_to_path(vault_.root()));
        fs::path doc_p = doc_rel.empty() ? root_p : (root_p / utf8_to_path(doc_rel)).parent_path();
        fs::path cand = fs::weakly_canonical(utf8_to_path(asset_rel).is_absolute() ? utf8_to_path(asset_rel) : (doc_p / utf8_to_path(asset_rel)));
        std::string root_s = path_to_utf8(root_p);
        std::string cand_s = path_to_utf8(cand);
        if (cand_s.rfind(root_s, 0) != 0) {
            return {403, "text/plain", "forbidden"};
        }
        std::error_code ec;
        if (!fs::exists(cand, ec) || !fs::is_regular_file(cand, ec)) {
            return {404, "text/plain", "not found"};
        }
        std::ifstream ifs(cand, std::ios::binary);
        std::ostringstream ss;
        ss << ifs.rdbuf();
        std::string ext = path_to_utf8(cand.extension());
        for (char& ch : ext) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        std::string mime = "application/octet-stream";
        if (ext == ".png") mime = "image/png";
        else if (ext == ".jpg" || ext == ".jpeg") mime = "image/jpeg";
        else if (ext == ".gif") mime = "image/gif";
        else if (ext == ".svg") mime = "image/svg+xml";
        else if (ext == ".webp") mime = "image/webp";
        else if (ext == ".bmp") mime = "image/bmp";
        return {200, mime, ss.str()};
    }

    if (req.method == "POST" && req.path == "/api/vault/upload-image") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string doc_rel = body.get("doc_path").as_string();
        std::string fname = body.get("filename").as_string("image.png");
        std::string data_url = body.get("data_url").as_string();
        size_t comma = data_url.find(',');
        if (comma == std::string::npos) {
            return {400, "application/json; charset=utf-8", make_error("Invalid image data").dump()};
        }
        std::string raw_bytes = decode_b64_bytes(data_url.substr(comma + 1));
        if (raw_bytes.empty()) {
            return {400, "application/json; charset=utf-8", make_error("Empty image payload").dump()};
        }
        std::string safe_name;
        for (unsigned char c : fname) {
            if (std::isalnum(c) || c == '.' || c == '_' || c == '-' || c >= 128) safe_name.push_back(static_cast<char>(c));
            else safe_name.push_back('_');
        }
        if (safe_name.empty()) safe_name = "image.png";
        auto stamp = std::to_string(now_ms() % 1000000);
        std::string final_name = stamp + "_" + safe_name;

        if (vault_.root().empty()) {
            return {400, "application/json; charset=utf-8", make_error("请先选择一个本地知识库文件夹").dump()};
        }
        fs::path root_p = utf8_to_path(vault_.root());
        fs::path doc_dir = doc_rel.empty() ? root_p : (root_p / utf8_to_path(doc_rel)).parent_path();
        fs::path assets_dir = doc_dir / "assets";
        std::error_code ec;
        fs::create_directories(assets_dir, ec);
        fs::path out_img = assets_dir / utf8_to_path(final_name);
        std::ofstream ofs(out_img, std::ios::binary);
        ofs.write(raw_bytes.data(), static_cast<std::streamsize>(raw_bytes.size()));
        ofs.close();

        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["rel_url"] = "assets/" + final_name;
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/vault/create") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string kind = body.get("kind").as_string("note");
        std::string rel = body.get("path").as_string();
        std::string err;
        bool ok = false;
        if (kind == "folder") {
            ok = vault_.create_folder(rel, &err);
        } else {
            std::string initial = body.get("markdown").as_string("# 新建笔记\n\n在此开始编写 Markdown、表格与 $$LaTeX$$ 公式。\n");
            ok = vault_.create_note(rel, initial, &err);
        }
        if (!ok) {
            return {400, "application/json; charset=utf-8", make_error(err).dump()};
        }
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["tree"] = vault_.scan_tree();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/vault/resolve-wikilink") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string name = body.get("name").as_string();
        std::string rel = vault_.resolve_wikilink(name);
        if (rel.empty()) {
            return {404, "application/json; charset=utf-8", make_error("未在知识库中找到双链目标: " + name).dump()};
        }
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["rel_path"] = rel;
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/vault/open-folder") {
        JsonValue body;
        std::string target = vault_.root();
        if (JsonValue::parse(req.body, body) && body.has("path") && !body.get("path").as_string().empty()) {
            target = body.get("path").as_string();
        }
        bool ok = open_in_file_manager(target);
        JsonValue res = JsonValue::make_object();
        res["ok"] = ok;
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/render-md") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string md = body.get("markdown").as_string();
        std::string doc_path = body.get("doc_path").as_string();
        MdDocument doc = parse_markdown(md);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["html"] = render_markdown_to_html(doc, false, "OmniWorkbench Document", doc_path);
        JsonValue formulas = JsonValue::make_array();
        for (const auto& f : extract_latex_blocks(md)) formulas.push_back(f);
        res["formulas"] = std::move(formulas);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/convert") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string source_path = body.get("source_path").as_string();
        std::string md_override = body.get("markdown").as_string();
        std::string target_fmt = body.get("target_format").as_string("docx");
        std::string out_path = body.get("out_path").as_string();
        bool save_as = body.get("save_as").as_bool(false);
        DocxExportContext dctx = build_docx_ctx(body);

        fs::path src_p = utf8_to_path(source_path.empty() ? "OmniNote.md" : source_path);
        std::string default_out_name = path_to_utf8(src_p.stem()) + "." + target_fmt;

        if (save_as) {
            std::string chosen = save_file_dialog("导出转换为 " + target_fmt, default_out_name, target_fmt);
            if (!chosen.empty()) {
                out_path = chosen;
            } else if (out_path.empty()) {
                JsonValue res = JsonValue::make_object();
                res["ok"] = false;
                res["cancelled"] = true;
                return {200, "application/json; charset=utf-8", res.dump()};
            }
        }

        if (out_path.empty()) {
            fs::path rel_out = src_p.parent_path() / utf8_to_path(default_out_name);
            out_path = path_to_utf8(rel_out);
            for (char& c : out_path) if (c == '\\') c = '/';
        }

        std::string err;
        if (!vault_.convert_document(source_path, md_override, target_fmt, out_path, &err, &dctx)) {
            return {500, "application/json; charset=utf-8", make_error(err).dump()};
        }

        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["out_path"] = out_path;
        res["target_format"] = target_fmt;
        res["tree"] = vault_.scan_tree();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/dict/search") {
        std::string q = get_query_param(req.query, "q");
        std::string tag = get_query_param(req.query, "tag");
        bool marked_only = (get_query_param(req.query, "marked") == "1");
        int limit = 50;
        int offset = 0;
        std::string lim_s = get_query_param(req.query, "limit");
        std::string off_s = get_query_param(req.query, "offset");
        if (!lim_s.empty()) {
            try { limit = std::max(1, std::min(500, std::stoi(lim_s))); } catch (...) {}
        }
        if (!off_s.empty()) {
            try { offset = std::max(0, std::stoi(off_s)); } catch (...) {}
        }
        auto list = dict_.search(q, limit, tag, marked_only, false, offset);
        int match_total = dict_.count_matches(q, tag, marked_only);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["total"] = dict_.total_count();
        res["match_total"] = match_total;
        res["offset"] = offset;
        res["limit"] = limit;
        JsonValue arr = JsonValue::make_array();
        for (const auto& e : list) arr.push_back(dict_.entry_to_json(e));
        res["results"] = std::move(arr);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/dict/similar") {
        std::string w = get_query_param(req.query, "w");
        std::string tag = get_query_param(req.query, "tag");
        int limit = 50;
        int offset = 0;
        std::string lim_s = get_query_param(req.query, "limit");
        std::string off_s = get_query_param(req.query, "offset");
        if (!lim_s.empty()) {
            try { limit = std::max(1, std::min(200, std::stoi(lim_s))); } catch (...) {}
        }
        if (!off_s.empty()) {
            try { offset = std::max(0, std::stoi(off_s)); } catch (...) {}
        }
        int match_total = 0;
        auto list = dict_.similar_words(w, tag, limit, offset, &match_total);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["anchor"] = w;
        res["total"] = dict_.total_count();
        res["match_total"] = match_total;
        res["offset"] = offset;
        res["limit"] = limit;
        JsonValue arr = JsonValue::make_array();
        for (const auto& e : list) arr.push_back(dict_.entry_to_json(e));
        res["results"] = std::move(arr);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/dict/by-phonetic") {
        std::string combo = get_query_param(req.query, "combo");
        std::string ipa = get_query_param(req.query, "ipa");
        int limit = 50;
        int offset = 0;
        std::string lim_s = get_query_param(req.query, "limit");
        std::string off_s = get_query_param(req.query, "offset");
        if (!lim_s.empty()) {
            try { limit = std::max(1, std::min(300, std::stoi(lim_s))); } catch (...) {}
        }
        if (!off_s.empty()) {
            try { offset = std::max(0, std::stoi(off_s)); } catch (...) {}
        }
        auto list = dict_.search_by_phonetic(combo, ipa, limit, offset);
        int match_total = dict_.count_by_phonetic(combo, ipa);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["combo"] = combo;
        res["ipa"] = ipa;
        res["total"] = dict_.total_count();
        res["match_total"] = match_total;
        res["offset"] = offset;
        res["limit"] = limit;
        JsonValue arr = JsonValue::make_array();
        for (const auto& e : list) arr.push_back(dict_.entry_to_json(e));
        res["results"] = std::move(arr);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/dict/import") {
        JsonValue body;
        JsonValue::parse(req.body, body);
        std::string path = body.get("path").as_string();
        std::string book_tag = body.get("book_tag").as_string();
        bool pick = body.get("pick").as_bool(false);

        if (pick && path.empty()) {
            path = open_file_dialog("选择词库文件 (kajweb/dict ZIP/JSON, KyleBing JSON, ECDICT CSV)",
                                    "Dictionary Files (*.json;*.csv;*.zip)", "*.json;*.csv;*.zip");
            if (path.empty()) {
                JsonValue res = JsonValue::make_object();
                res["ok"] = false;
                res["cancelled"] = true;
                return {200, "application/json; charset=utf-8", res.dump()};
            }
        }

        int imported = 0;
        std::string err;
        if (!dict_.import_dictionary_file(path, book_tag, imported, &err)) {
            return {400, "application/json; charset=utf-8", make_error(err).dump()};
        }
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["imported"] = imported;
        res["total"] = dict_.total_count();
        res["file"] = path;
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/dict/word") {
        std::string w = get_query_param(req.query, "w");
        DictEntry e;
        if (!dict_.get_word(w, e)) {
            return {404, "application/json; charset=utf-8", make_error("Word not found").dump()};
        }
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["entry"] = dict_.entry_to_json(e);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/dict/mark") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string word = body.get("word").as_string();
        bool marked = body.get("marked").as_bool(true);
        dict_.set_marked(word, marked);
        if (marked) config_.mutable_get().marked_words.insert(word);
        else config_.mutable_get().marked_words.erase(word);
        config_.save();
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["marked"] = marked;
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/dict/favorite") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string word = body.get("word").as_string();
        bool fav = body.get("favorite").as_bool(true);
        dict_.set_favorite(word, fav);
        if (fav) config_.mutable_get().favorite_words.insert(word);
        else config_.mutable_get().favorite_words.erase(word);
        config_.save();
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["favorite"] = fav;
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/dict/note") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string word = body.get("word").as_string();
        std::string note = body.get("note").as_string();
        dict_.save_note(word, note);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/dict/append-vocab") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        std::string word = body.get("word").as_string();
        std::string ipa = body.get("ipa").as_string();
        std::string pos = body.get("pos").as_string();
        std::string note = body.get("note").as_string();
        dict_.save_note(word, note);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        if (vault_.root().empty()) {
            res["synced_to_vault"] = false;
            return {200, "application/json; charset=utf-8", res.dump()};
        }
        std::string err;
        if (!vault_.append_vocabulary_entry(word, ipa, pos, note, &err)) {
            return {500, "application/json; charset=utf-8", make_error(err).dump()};
        }
        res["synced_to_vault"] = true;
        res["tree"] = vault_.scan_tree();
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/reading/search") {
        std::string q = get_query_param(req.query, "q");
        std::string source = get_query_param(req.query, "source");
        std::string kind = get_query_param(req.query, "kind");
        int limit = 18;
        int offset = 0;
        std::string lim_s = get_query_param(req.query, "limit");
        std::string off_s = get_query_param(req.query, "offset");
        if (!lim_s.empty()) {
            try { limit = std::max(1, std::min(100, std::stoi(lim_s))); } catch (...) {}
        }
        if (!off_s.empty()) {
            try { offset = std::max(0, std::stoi(off_s)); } catch (...) {}
        }
        auto list = dict_.search_readings(q, source, kind, limit, offset);
        int match_articles = 0;
        int match_questions = 0;
        dict_.reading_stats(q, source, kind, match_articles, match_questions);
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["total"] = dict_.reading_count();
        res["total_questions"] = dict_.total_reading_questions();
        res["match_total"] = match_articles;
        res["match_questions"] = match_questions;
        res["offset"] = offset;
        res["limit"] = limit;
        JsonValue arr = JsonValue::make_array();
        for (const auto& r : list) arr.push_back(dict_.reading_to_json(r, false));
        res["results"] = std::move(arr);
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "GET" && req.path == "/api/reading/item") {
        std::string id_s = get_query_param(req.query, "id");
        int id = 0;
        try { id = std::stoi(id_s); } catch (...) { id = 0; }
        ReadingArticle r;
        if (id <= 0 || !dict_.get_reading(id, r)) {
            return {404, "application/json; charset=utf-8", make_error("Reading article not found").dump()};
        }
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["article"] = dict_.reading_to_json(r, true);
        res["marks"] = dict_.marks_to_json(dict_.get_reading_marks(id));
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    if (req.method == "POST" && req.path == "/api/reading/mark") {
        JsonValue body;
        if (!JsonValue::parse(req.body, body)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid JSON").dump()};
        }
        ReadingWordMark m;
        m.article_id = body.get("article_id").as_int(0);
        m.word = body.get("word").as_string();
        m.surface = body.get("surface").as_string();
        m.color = body.get("color").as_string();
        m.meaning = body.get("meaning").as_string();
        m.context = body.get("context").as_string();
        if (!dict_.set_reading_mark(m)) {
            return {400, "application/json; charset=utf-8", make_error("Invalid reading mark parameters").dump()};
        }
        JsonValue res = JsonValue::make_object();
        res["ok"] = true;
        res["marks"] = dict_.marks_to_json(dict_.get_reading_marks(m.article_id));
        return {200, "application/json; charset=utf-8", res.dump()};
    }

    return {404, "application/json; charset=utf-8", make_error("Unknown endpoint: " + req.path).dump()};
}

} // namespace omni
