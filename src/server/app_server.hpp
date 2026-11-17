#pragma once

#include "../core/config.hpp"
#include "../core/dict_manager.hpp"
#include "../core/vault_manager.hpp"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace omni {

/**
 * Embedded localhost HTTP/JSON server connecting the C++17 core engine
 * (Vault, MD/DOCX/PDF converters, SQLite3 dictionary, native OS dialogs)
 * to the OmniWorkbench desktop UI.
 */
class AppServer {
public:
    explicit AppServer(std::string ui_html);
    ~AppServer();

    bool start(uint16_t preferred_port = 0);
    void stop();
    void wait_until_stopped();

    uint16_t port() const { return port_; }
    bool is_running() const { return running_.load(); }

    /**
     * If auto_exit_on_idle is true (desktop app mode), the server exits shortly
     * after the UI window closes and sends /api/shutdown or stops pinging.
     */
    void set_auto_exit_on_close(bool enabled) { auto_exit_on_close_ = enabled; }

private:
    std::string ui_html_;
    ConfigManager config_;
    VaultManager vault_;
    DictManager dict_;
    std::mutex state_mutex_;

    std::atomic<bool> running_{false};
    std::atomic<bool> shutdown_requested_{false};
    bool auto_exit_on_close_ = false;
    uint16_t port_ = 0;

#ifdef _WIN32
    uintptr_t listen_sock_ = ~uintptr_t(0);
#else
    int listen_sock_ = -1;
#endif
    std::thread accept_thread_;

    std::atomic<int64_t> last_ping_ms_{0};
    std::atomic<bool> has_seen_client_{false};

    void accept_loop();
    void handle_client(uintptr_t client_sock);

    struct HttpRequest {
        std::string method;
        std::string path;
        std::string query;
        std::string body;
    };

    struct HttpResponse {
        int status = 200;
        std::string content_type = "application/json; charset=utf-8";
        std::string body;
    };

    HttpResponse route_request(const HttpRequest& req);
    static std::string url_decode(const std::string& s);
    static std::string get_query_param(const std::string& query, const std::string& key);
};

} // namespace omni
