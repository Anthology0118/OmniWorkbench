#include "ds_manager.hpp"

#include "ds_seed_ch0_ch3.hpp"
#include "ds_seed_ch4_ch6.hpp"
#include "ds_seed_ch7_ch10.hpp"
#include "ds_seed_ch11_ch13.hpp"
#include "platform_utils.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef OMNI_USE_SQLITE3
  #include <sqlite3.h>
#endif

#ifdef _WIN32
  #ifndef NOMINMAX
    #define NOMINMAX
  #endif
  #include <windows.h>
#else
  #include <fcntl.h>
  #include <signal.h>
  #include <sys/types.h>
  #include <sys/wait.h>
  #include <thread>
  #include <unistd.h>
#endif

namespace omni {

namespace fs = std::filesystem;

namespace {

std::string to_lower_ascii(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string normalize_output(const std::string& raw) {
    std::string norm;
    norm.reserve(raw.size());
    for (char c : raw) {
        if (c != '\r') norm.push_back(c);
    }
    std::vector<std::string> lines;
    std::istringstream iss(norm);
    std::string line;
    while (std::getline(iss, line)) {
        while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) {
            line.pop_back();
        }
        lines.push_back(line);
    }
    while (!lines.empty() && lines.back().empty()) {
        lines.pop_back();
    }
    std::ostringstream oss;
    for (size_t i = 0; i < lines.size(); ++i) {
        if (i > 0) oss << "\n";
        oss << lines[i];
    }
    return oss.str();
}

struct SubprocessResult {
    bool launched = false;
    bool timed_out = false;
    int exit_code = -1;
    double elapsed_ms = 0.0;
};

SubprocessResult exec_with_io_capture(
    const std::string& exe_path,
    const std::vector<std::string>& args,
    const std::string& stdin_path,
    const std::string& stdout_path,
    const std::string& stderr_path,
    int timeout_ms) {
    SubprocessResult res;
    auto t0 = std::chrono::steady_clock::now();

#ifdef _WIN32
    std::ostringstream cmd;
    cmd << "\"" << exe_path << "\"";
    for (const auto& a : args) {
        cmd << " \"" << a << "\"";
    }
    auto to_wide = [](const std::string& s) {
        if (s.empty()) return std::wstring();
        int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), &w[0], n);
        return w;
    };

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE h_in = INVALID_HANDLE_VALUE;
    if (!stdin_path.empty()) {
        std::wstring win = to_wide(stdin_path);
        h_in = CreateFileW(win.c_str(), GENERIC_READ, FILE_SHARE_READ, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    } else {
        h_in = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    }
    std::wstring wout = to_wide(stdout_path);
    std::wstring werr = to_wide(stderr_path);
    HANDLE h_out = CreateFileW(wout.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE h_err = CreateFileW(werr.c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_HIDE;
    si.hStdInput = (h_in != INVALID_HANDLE_VALUE) ? h_in : GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = (h_out != INVALID_HANDLE_VALUE) ? h_out : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError = (h_err != INVALID_HANDLE_VALUE) ? h_err : GetStdHandle(STD_ERROR_HANDLE);

    std::wstring wcmd = to_wide(cmd.str());
    PROCESS_INFORMATION pi{};
    if (CreateProcessW(nullptr, &wcmd[0], nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        res.launched = true;
        DWORD wait_rc = WaitForSingleObject(pi.hProcess, static_cast<DWORD>(timeout_ms));
        if (wait_rc == WAIT_TIMEOUT) {
            res.timed_out = true;
            TerminateProcess(pi.hProcess, 124);
            WaitForSingleObject(pi.hProcess, 1000);
        }
        DWORD code = 1;
        GetExitCodeProcess(pi.hProcess, &code);
        res.exit_code = static_cast<int>(code);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
    if (h_in != INVALID_HANDLE_VALUE) CloseHandle(h_in);
    if (h_out != INVALID_HANDLE_VALUE) CloseHandle(h_out);
    if (h_err != INVALID_HANDLE_VALUE) CloseHandle(h_err);
#else
    pid_t pid = fork();
    if (pid == 0) {
        if (!stdin_path.empty()) {
            int fdi = open(stdin_path.c_str(), O_RDONLY);
            if (fdi >= 0) { dup2(fdi, STDIN_FILENO); close(fdi); }
        } else {
            int fdn = open("/dev/null", O_RDONLY);
            if (fdn >= 0) { dup2(fdn, STDIN_FILENO); close(fdn); }
        }
        int fdo = open(stdout_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fdo >= 0) { dup2(fdo, STDOUT_FILENO); close(fdo); }
        int fde = open(stderr_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fde >= 0) { dup2(fde, STDERR_FILENO); close(fde); }

        std::vector<char*> argv;
        argv.push_back(const_cast<char*>(exe_path.c_str()));
        for (const auto& a : args) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);
        execv(exe_path.c_str(), argv.data());
        _exit(127);
    }
    if (pid > 0) {
        res.launched = true;
        auto deadline = t0 + std::chrono::milliseconds(timeout_ms);
        while (true) {
            int status = 0;
            pid_t w = waitpid(pid, &status, WNOHANG);
            if (w == pid) {
                if (WIFEXITED(status)) res.exit_code = WEXITSTATUS(status);
                else if (WIFSIGNALED(status)) res.exit_code = 128 + WTERMSIG(status);
                break;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                res.timed_out = true;
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                res.exit_code = 124;
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    }
#endif
    auto t1 = std::chrono::steady_clock::now();
    res.elapsed_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return res;
}

} // namespace

DsManager::DsManager(std::string db_path) : db_path_(std::move(db_path)) {
    if (db_path_.empty()) {
        db_path_ = path_to_utf8(utf8_to_path(get_app_data_dir()) / "omni_ds_course.db");
    }
}

DsManager::~DsManager() {
#ifdef OMNI_USE_SQLITE3
    if (db_handle_) {
        sqlite3_close(static_cast<sqlite3*>(db_handle_));
        db_handle_ = nullptr;
    }
#endif
}

bool DsManager::exec_sql(const std::string& sql) {
#ifdef OMNI_USE_SQLITE3
    if (!db_handle_) return false;
    char* err_msg = nullptr;
    int rc = sqlite3_exec(static_cast<sqlite3*>(db_handle_), sql.c_str(), nullptr, nullptr, &err_msg);
    if (err_msg) sqlite3_free(err_msg);
    return rc == SQLITE_OK;
#else
    (void)sql;
    return false;
#endif
}

void DsManager::seed_curriculum() {
    chapters_.clear();
    lessons_.clear();
    ds_seed::seed_ch0_ch3(chapters_, lessons_);
    ds_seed::seed_ch4_ch6(chapters_, lessons_);
    ds_seed::seed_ch7_ch10(chapters_, lessons_);
    ds_seed::seed_ch11_ch13(chapters_, lessons_);
}

bool DsManager::init() {
    seed_curriculum();

#ifdef OMNI_USE_SQLITE3
    std::error_code ec;
    fs::path p = utf8_to_path(db_path_);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) == SQLITE_OK) {
        db_handle_ = db;
        sqlite_ready_ = true;

        exec_sql(
            "CREATE TABLE IF NOT EXISTS ds_chapters ("
            "  id TEXT PRIMARY KEY,"
            "  sort_order INTEGER,"
            "  category TEXT,"
            "  title TEXT,"
            "  subtitle TEXT"
            ");"
        );
        exec_sql(
            "CREATE TABLE IF NOT EXISTS ds_lessons ("
            "  id TEXT PRIMARY KEY,"
            "  chapter_id TEXT,"
            "  sort_order INTEGER,"
            "  title TEXT,"
            "  source_ref TEXT,"
            "  time_comp TEXT,"
            "  space_comp TEXT,"
            "  viz_type TEXT,"
            "  viz_label TEXT,"
            "  tutorial_html TEXT,"
            "  code_template TEXT"
            ");"
        );
        exec_sql(
            "CREATE TABLE IF NOT EXISTS ds_exercises ("
            "  id TEXT PRIMARY KEY,"
            "  lesson_id TEXT,"
            "  sort_order INTEGER,"
            "  title TEXT,"
            "  mode TEXT,"
            "  prompt TEXT,"
            "  starter_code TEXT,"
            "  solution_code TEXT,"
            "  sample_input TEXT,"
            "  expected_output TEXT,"
            "  tests_json TEXT"
            ");"
        );
        exec_sql(
            "CREATE TABLE IF NOT EXISTS ds_progress ("
            "  key_id TEXT PRIMARY KEY,"
            "  mastered INTEGER DEFAULT 0,"
            "  note TEXT DEFAULT '',"
            "  completed INTEGER DEFAULT 0,"
            "  user_code TEXT DEFAULT '',"
            "  last_status TEXT DEFAULT ''"
            ");"
        );

        save_to_sqlite();
        load_from_sqlite();
        return true;
    }
#endif
    load_fallback_progress();
    return true;
}

void DsManager::save_to_sqlite() {
#ifdef OMNI_USE_SQLITE3
    if (!db_handle_) return;
    sqlite3* db = static_cast<sqlite3*>(db_handle_);
    exec_sql("BEGIN TRANSACTION;");
    exec_sql("DELETE FROM ds_exercises;");
    exec_sql("DELETE FROM ds_lessons;");
    exec_sql("DELETE FROM ds_chapters;");

    sqlite3_stmt* ch_st = nullptr;
    if (sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO ds_chapters(id, sort_order, category, title, subtitle) VALUES(?1, ?2, ?3, ?4, ?5);",
            -1, &ch_st, nullptr) == SQLITE_OK) {
        for (const auto& ch : chapters_) {
            sqlite3_bind_text(ch_st, 1, ch.id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(ch_st, 2, ch.sort_order);
            sqlite3_bind_text(ch_st, 3, ch.category.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ch_st, 4, ch.title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(ch_st, 5, ch.subtitle.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(ch_st);
            sqlite3_reset(ch_st);
        }
        sqlite3_finalize(ch_st);
    }

    sqlite3_stmt* les_st = nullptr;
    if (sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO ds_lessons(id, chapter_id, sort_order, title, source_ref, time_comp, space_comp, viz_type, viz_label, tutorial_html, code_template) "
            "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);",
            -1, &les_st, nullptr) == SQLITE_OK) {
        sqlite3_stmt* ex_st = nullptr;
        sqlite3_prepare_v2(db,
            "INSERT OR REPLACE INTO ds_exercises(id, lesson_id, sort_order, title, mode, prompt, starter_code, solution_code, sample_input, expected_output, tests_json) "
            "VALUES(?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11);",
            -1, &ex_st, nullptr);

        for (const auto& l : lessons_) {
            sqlite3_bind_text(les_st, 1, l.id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 2, l.chapter_id.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(les_st, 3, l.sort_order);
            sqlite3_bind_text(les_st, 4, l.title.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 5, l.source_ref.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 6, l.time_comp.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 7, l.space_comp.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 8, l.viz_type.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 9, l.viz_label.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 10, l.tutorial_html.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(les_st, 11, l.code_template.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(les_st);
            sqlite3_reset(les_st);

            if (ex_st) {
                for (const auto& ex : l.exercises) {
                    sqlite3_bind_text(ex_st, 1, ex.id.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 2, ex.lesson_id.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_int(ex_st, 3, ex.sort_order);
                    sqlite3_bind_text(ex_st, 4, ex.title.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 5, ex.mode.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 6, ex.prompt.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 7, ex.starter_code.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 8, ex.solution_code.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 9, ex.sample_input.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 10, ex.expected_output.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(ex_st, 11, ex.tests_json.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(ex_st);
                    sqlite3_reset(ex_st);
                }
            }
        }
        if (ex_st) sqlite3_finalize(ex_st);
        sqlite3_finalize(les_st);
    }
    exec_sql("COMMIT;");
#endif
}

void DsManager::load_from_sqlite() {
#ifdef OMNI_USE_SQLITE3
    if (!db_handle_) return;
    sqlite3* db = static_cast<sqlite3*>(db_handle_);

    auto col_str = [](sqlite3_stmt* s, int col) -> std::string {
        const unsigned char* p = sqlite3_column_text(s, col);
        return p ? reinterpret_cast<const char*>(p) : "";
    };

    // Read persisted lesson & exercise progress from ds_progress
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT key_id, mastered, note, completed, user_code, last_status FROM ds_progress;", -1, &st, nullptr) == SQLITE_OK) {
        while (sqlite3_step(st) == SQLITE_ROW) {
            std::string kid = col_str(st, 0);
            bool mast = sqlite3_column_int(st, 1) != 0;
            std::string note = col_str(st, 2);
            bool comp = sqlite3_column_int(st, 3) != 0;
            std::string ucode = col_str(st, 4);
            std::string lstat = col_str(st, 5);

            if (kid.rfind("les:", 0) == 0) {
                std::string lid = kid.substr(4);
                for (auto& l : lessons_) {
                    if (l.id == lid) {
                        l.mastered = mast;
                        l.note = note;
                        break;
                    }
                }
            } else if (kid.rfind("ex:", 0) == 0) {
                std::string eid = kid.substr(3);
                for (auto& l : lessons_) {
                    for (auto& ex : l.exercises) {
                        if (ex.id == eid) {
                            ex.completed = comp;
                            if (!ucode.empty()) ex.user_code = ucode;
                            ex.last_status = lstat;
                        }
                    }
                }
            }
        }
        sqlite3_finalize(st);
    }
#endif
}

void DsManager::load_fallback_progress() {
    std::string ppath = path_to_utf8(utf8_to_path(db_path_).parent_path() / "omni_ds_progress.json");
    std::string raw;
    if (!read_file_utf8(ppath, raw)) return;
    JsonValue root;
    if (!JsonValue::parse(raw, root) || !root.is_object()) return;

    if (root.get("lessons").is_object()) {
        for (auto& l : lessons_) {
            if (root.get("lessons").has(l.id)) {
                const auto& lv = root.get("lessons").get(l.id);
                l.mastered = lv.get("mastered").as_bool(false);
                l.note = lv.get("note").as_string("");
            }
        }
    }
    if (root.get("exercises").is_object()) {
        for (auto& l : lessons_) {
            for (auto& ex : l.exercises) {
                if (root.get("exercises").has(ex.id)) {
                    const auto& ev = root.get("exercises").get(ex.id);
                    ex.completed = ev.get("completed").as_bool(false);
                    std::string uc = ev.get("user_code").as_string("");
                    if (!uc.empty()) ex.user_code = uc;
                    ex.last_status = ev.get("last_status").as_string("");
                }
            }
        }
    }
}

void DsManager::save_fallback_progress() const {
    JsonValue root = JsonValue::make_object();
    JsonValue les_obj = JsonValue::make_object();
    JsonValue ex_obj = JsonValue::make_object();
    for (const auto& l : lessons_) {
        if (l.mastered || !l.note.empty()) {
            JsonValue lv = JsonValue::make_object();
            lv["mastered"] = l.mastered;
            lv["note"] = l.note;
            les_obj[l.id] = std::move(lv);
        }
        for (const auto& ex : l.exercises) {
            if (ex.completed || ex.user_code != ex.starter_code || !ex.last_status.empty()) {
                JsonValue ev = JsonValue::make_object();
                ev["completed"] = ex.completed;
                ev["user_code"] = ex.user_code;
                ev["last_status"] = ex.last_status;
                ex_obj[ex.id] = std::move(ev);
            }
        }
    }
    root["lessons"] = std::move(les_obj);
    root["exercises"] = std::move(ex_obj);
    std::string ppath = path_to_utf8(utf8_to_path(db_path_).parent_path() / "omni_ds_progress.json");
    write_file_utf8(ppath, root.dump(2));
}

int DsManager::completed_exercises() const {
    int c = 0;
    for (const auto& l : lessons_) {
        for (const auto& ex : l.exercises) {
            if (ex.completed) ++c;
        }
    }
    return c;
}

int DsManager::total_exercises() const {
    int c = 0;
    for (const auto& l : lessons_) {
        c += static_cast<int>(l.exercises.size());
    }
    return c;
}

JsonValue DsManager::get_catalog(const std::string& query, const std::string& category) const {
    std::string ql = to_lower_ascii(query);

    JsonValue res = JsonValue::make_object();
    res["ok"] = true;
    res["db_mode"] = sqlite_ready_ ? "sqlite3" : "json_fallback";
    res["total_lessons"] = total_lessons();
    res["total_exercises"] = total_exercises();
    res["completed_exercises"] = completed_exercises();
    res["compiler_available"] = !find_cpp_compiler().empty();

    JsonValue ch_arr = JsonValue::make_array();
    for (const auto& ch : chapters_) {
        if (!category.empty() && category != "全部" && ch.category != category) continue;

        JsonValue les_arr = JsonValue::make_array();
        int ch_ex_total = 0;
        int ch_ex_done = 0;
        int ch_mastered = 0;

        for (const auto& l : lessons_) {
            if (l.chapter_id != ch.id) continue;
            if (!ql.empty()) {
                std::string hay = to_lower_ascii(l.title + " " + l.source_ref + " " + l.viz_label + " " + l.code_template);
                if (hay.find(ql) == std::string::npos) continue;
            }
            int ex_done = 0;
            for (const auto& ex : l.exercises) {
                if (ex.completed) ++ex_done;
            }
            ch_ex_total += static_cast<int>(l.exercises.size());
            ch_ex_done += ex_done;
            if (l.mastered) ++ch_mastered;

            JsonValue lo = JsonValue::make_object();
            lo["id"] = l.id;
            lo["title"] = l.title;
            lo["source_ref"] = l.source_ref;
            lo["time_comp"] = l.time_comp;
            lo["space_comp"] = l.space_comp;
            lo["mastered"] = l.mastered;
            lo["exercise_total"] = static_cast<int>(l.exercises.size());
            lo["exercise_done"] = ex_done;
            les_arr.push_back(std::move(lo));
        }

        if (les_arr.arr_val.empty()) continue;

        JsonValue co = JsonValue::make_object();
        co["id"] = ch.id;
        co["category"] = ch.category;
        co["title"] = ch.title;
        co["subtitle"] = ch.subtitle;
        co["lesson_count"] = static_cast<int>(les_arr.arr_val.size());
        co["mastered_count"] = ch_mastered;
        co["exercise_total"] = ch_ex_total;
        co["exercise_done"] = ch_ex_done;
        co["lessons"] = std::move(les_arr);
        ch_arr.push_back(std::move(co));
    }

    res["chapters"] = std::move(ch_arr);
    return res;
}

bool DsManager::get_lesson(const std::string& lesson_id, DsLesson& out) const {
    for (const auto& l : lessons_) {
        if (l.id == lesson_id) {
            out = l;
            return true;
        }
    }
    return false;
}

JsonValue DsManager::lesson_to_json(const DsLesson& l) const {
    JsonValue obj = JsonValue::make_object();
    obj["id"] = l.id;
    obj["chapter_id"] = l.chapter_id;
    obj["title"] = l.title;
    obj["source_ref"] = l.source_ref;
    obj["time_comp"] = l.time_comp;
    obj["space_comp"] = l.space_comp;
    obj["viz_type"] = l.viz_type;
    obj["viz_label"] = l.viz_label;
    obj["tutorial_html"] = l.tutorial_html;
    obj["code_template"] = l.code_template;
    obj["mastered"] = l.mastered;
    obj["note"] = l.note;

    JsonValue exs = JsonValue::make_array();
    for (const auto& ex : l.exercises) {
        JsonValue eo = JsonValue::make_object();
        eo["id"] = ex.id;
        eo["lesson_id"] = ex.lesson_id;
        eo["title"] = ex.title;
        eo["mode"] = ex.mode;
        eo["prompt"] = ex.prompt;
        eo["starter_code"] = ex.starter_code;
        eo["solution_code"] = ex.solution_code;
        eo["sample_input"] = ex.sample_input;
        eo["expected_output"] = ex.expected_output;
        eo["tests_json"] = ex.tests_json;
        eo["completed"] = ex.completed;
        eo["user_code"] = ex.user_code.empty() ? ex.starter_code : ex.user_code;
        eo["last_status"] = ex.last_status;
        exs.push_back(std::move(eo));
    }
    obj["exercises"] = std::move(exs);
    return obj;
}

bool DsManager::save_progress(const std::string& lesson_id,
                              bool mastered,
                              const std::string& note,
                              const std::string& exercise_id,
                              const std::string& user_code,
                              const std::string& last_status,
                              bool mark_completed) {
    bool found = false;
    for (auto& l : lessons_) {
        if (!lesson_id.empty() && l.id == lesson_id) {
            l.mastered = mastered;
            l.note = note;
            found = true;
        }
        if (!exercise_id.empty()) {
            for (auto& ex : l.exercises) {
                if (ex.id == exercise_id) {
                    if (!user_code.empty()) ex.user_code = user_code;
                    if (!last_status.empty()) ex.last_status = last_status;
                    if (mark_completed) ex.completed = true;
                    found = true;
                }
            }
        }
    }

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_ && db_handle_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db,
                "INSERT OR REPLACE INTO ds_progress(key_id, mastered, note, completed, user_code, last_status) VALUES(?1, ?2, ?3, ?4, ?5, ?6);",
                -1, &st, nullptr) == SQLITE_OK) {
            if (!lesson_id.empty()) {
                std::string k = "les:" + lesson_id;
                sqlite3_bind_text(st, 1, k.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 2, mastered ? 1 : 0);
                sqlite3_bind_text(st, 3, note.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 4, 0);
                sqlite3_bind_text(st, 5, "", -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 6, "", -1, SQLITE_TRANSIENT);
                sqlite3_step(st);
                sqlite3_reset(st);
            }
            if (!exercise_id.empty()) {
                std::string k = "ex:" + exercise_id;
                sqlite3_bind_text(st, 1, k.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 2, 0);
                sqlite3_bind_text(st, 3, "", -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 4, mark_completed ? 1 : 0);
                sqlite3_bind_text(st, 5, user_code.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 6, last_status.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(st);
                sqlite3_reset(st);
            }
            sqlite3_finalize(st);
        }
        return true;
    }
#endif
    save_fallback_progress();
    return found || !lesson_id.empty() || !exercise_id.empty();
}

std::string DsManager::find_cpp_compiler() {
#ifdef _WIN32
    wchar_t buf[MAX_PATH] = {0};
    if (SearchPathW(nullptr, L"g++.exe", nullptr, MAX_PATH, buf, nullptr) > 0) {
        std::string found = path_to_utf8(fs::path(buf));
        if (!found.empty()) return found;
    }
    const char* candidates[] = {
        "D:\\msys\\ucrt64\\bin\\g++.exe",
        "D:\\msys\\mingw64\\bin\\g++.exe",
        "C:\\msys64\\ucrt64\\bin\\g++.exe",
        "C:\\msys64\\mingw64\\bin\\g++.exe",
        "C:\\mingw64\\bin\\g++.exe",
        "C:\\TDM-GCC-64\\bin\\g++.exe"
    };
    std::error_code ec;
    for (const char* c : candidates) {
        if (fs::exists(utf8_to_path(c), ec)) return c;
    }
    if (SearchPathW(nullptr, L"clang++.exe", nullptr, MAX_PATH, buf, nullptr) > 0) {
        std::string found = path_to_utf8(fs::path(buf));
        if (!found.empty()) return found;
    }
    return "";
#else
    const char* candidates[] = {
        "/usr/bin/g++",
        "/usr/local/bin/g++",
        "/usr/bin/clang++"
    };
    std::error_code ec;
    for (const char* c : candidates) {
        if (fs::exists(utf8_to_path(c), ec)) return c;
    }
    return "";
#endif
}

DsRunResult DsManager::run_and_grade(const std::string& exercise_id,
                                     const std::string& user_code,
                                     const std::string& custom_input,
                                     bool run_tests) {
    DsRunResult res;
    res.case_results = JsonValue::make_array();

    DsExercise* target_ex = nullptr;
    for (auto& l : lessons_) {
        for (auto& ex : l.exercises) {
            if (ex.id == exercise_id) {
                target_ex = &ex;
                break;
            }
        }
        if (target_ex) break;
    }

    std::string compiler = find_cpp_compiler();
    res.compiler_used = compiler;
    if (compiler.empty()) {
        res.ok = false;
        res.compiled = false;
        res.stderr_text = "未在本机检测到 g++ / clang++ 编译器。请确认已安装 MinGW-w64 / MSYS2（如 D:\\msys\\ucrt64\\bin\\g++.exe）或系统 PATH 中包含 g++。";
        return res;
    }

    std::error_code ec;
    static std::atomic<int> run_seq{0};
    int seq = ++run_seq;
    fs::path tmp_dir = utf8_to_path(get_temp_dir()) / ("omni_ds_run_" + std::to_string(seq));
    fs::create_directories(tmp_dir, ec);

    fs::path src_p = tmp_dir / "main.cpp";
#ifdef _WIN32
    fs::path bin_p = tmp_dir / "main.exe";
#else
    fs::path bin_p = tmp_dir / "main.bin";
#endif
    fs::path comp_out = tmp_dir / "compile_out.txt";
    fs::path comp_err = tmp_dir / "compile_err.txt";
    fs::path run_in = tmp_dir / "stdin.txt";
    fs::path run_out = tmp_dir / "stdout.txt";
    fs::path run_err = tmp_dir / "stderr.txt";

    write_file_utf8(path_to_utf8(src_p), user_code);

    std::vector<std::string> c_args = {
        "-std=c++17", "-O2", "-Wall",
#ifdef _WIN32
        "-static",
#endif
        path_to_utf8(src_p), "-o", path_to_utf8(bin_p)
    };
    SubprocessResult cres = exec_with_io_capture(
        compiler, c_args, "", path_to_utf8(comp_out), path_to_utf8(comp_err), 15000);
    res.compile_ms = cres.elapsed_ms;

    if (!cres.launched || cres.exit_code != 0 || !fs::exists(bin_p, ec)) {
        res.ok = true;
        res.compiled = false;
        read_file_utf8(path_to_utf8(comp_err), res.stderr_text);
        if (res.stderr_text.empty()) {
            res.stderr_text = "编译失败（exit code " + std::to_string(cres.exit_code) + "）";
        }
        if (target_ex) {
            save_progress("", false, "", exercise_id, user_code, "CE", false);
        }
        fs::remove_all(tmp_dir, ec);
        return res;
    }

    res.ok = true;
    res.compiled = true;

    // Parse test cases if run_tests == true and exercise has tests_json
    JsonValue parsed_cases;
    bool has_cases = run_tests && target_ex && !target_ex->tests_json.empty() &&
                     JsonValue::parse(target_ex->tests_json, parsed_cases) &&
                     parsed_cases.is_array() && !parsed_cases.arr_val.empty();

    if (!has_cases) {
        std::string stdin_data = custom_input;
        if (stdin_data.empty() && target_ex) stdin_data = target_ex->sample_input;
        write_file_utf8(path_to_utf8(run_in), stdin_data);
        SubprocessResult rres = exec_with_io_capture(
            path_to_utf8(bin_p), {}, path_to_utf8(run_in), path_to_utf8(run_out), path_to_utf8(run_err), 2500);
        read_file_utf8(path_to_utf8(run_out), res.stdout_text);
        read_file_utf8(path_to_utf8(run_err), res.stderr_text);
        if (rres.timed_out) {
            res.stderr_text += "\n[Time Limit Exceeded > 2.5s]";
        }
        if (target_ex) {
            save_progress("", false, "", exercise_id, user_code,
                          rres.timed_out ? "TLE" : (rres.exit_code == 0 ? "RAN" : "RE"), false);
        }
        fs::remove_all(tmp_dir, ec);
        return res;
    }

    int pass_cnt = 0;
    int tot_cnt = static_cast<int>(parsed_cases.arr_val.size());
    for (size_t i = 0; i < parsed_cases.arr_val.size(); ++i) {
        const JsonValue& tc = parsed_cases.arr_val[i];
        std::string label = tc.get("label").as_string("用例 " + std::to_string(i + 1));
        std::string inp = tc.get("input").as_string("");
        std::string exp = normalize_output(tc.get("expected").as_string(""));

        write_file_utf8(path_to_utf8(run_in), inp);
        SubprocessResult rres = exec_with_io_capture(
            path_to_utf8(bin_p), {}, path_to_utf8(run_in), path_to_utf8(run_out), path_to_utf8(run_err), 2500);

        std::string out_raw, err_raw;
        read_file_utf8(path_to_utf8(run_out), out_raw);
        read_file_utf8(path_to_utf8(run_err), err_raw);
        std::string act = normalize_output(out_raw);

        bool passed = (!rres.timed_out && rres.exit_code == 0 && act == exp);
        if (passed) ++pass_cnt;

        if (i == 0) {
            res.stdout_text = out_raw;
            res.stderr_text = err_raw;
            if (rres.timed_out) res.stderr_text += "\n[TLE > 2.5s]";
        }

        JsonValue cr = JsonValue::make_object();
        cr["label"] = label;
        cr["passed"] = passed;
        cr["timed_out"] = rres.timed_out;
        cr["input"] = inp;
        cr["expected"] = exp;
        cr["actual"] = rres.timed_out ? "[TLE 超时]" : act;
        cr["time_ms"] = rres.elapsed_ms;
        res.case_results.push_back(std::move(cr));
    }

    res.passed_count = pass_cnt;
    res.total_count = tot_cnt;
    res.all_passed = (pass_cnt == tot_cnt && tot_cnt > 0);

    if (target_ex) {
        save_progress("", false, "", exercise_id, user_code,
                      res.all_passed ? "PASS" : "WA", res.all_passed);
    }

    fs::remove_all(tmp_dir, ec);
    return res;
}

} // namespace omni
