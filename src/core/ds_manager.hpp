#pragma once

#include "json.hpp"

#include <string>
#include <vector>

namespace omni {

struct DsChapter {
    std::string id;        // e.g. "ch0", "ch1", ..., "ch11"
    int sort_order = 0;
    std::string category;  // e.g. "前导/基础", "STL与算法", "线性/搜索", "树与图论", "DP与高阶"
    std::string title;     // e.g. "第 6 章｜线性数据结构：链表、栈与队列"
    std::string subtitle;  // e.g. "《算法笔记》Ch7 · 算法集训核心专题"
};

struct DsExercise {
    std::string id;
    std::string lesson_id;
    int sort_order = 0;
    std::string title;
    std::string mode;         // "completion" | "scratch" | "free"
    std::string prompt;
    std::string starter_code; // code with // TODO or full starter
    std::string driver_prefix;// hidden harness prefix (optional) or empty if starter is standalone
    std::string driver_suffix;// hidden harness main() if mode runs function, or empty if full main()
    std::string solution_code;
    std::string sample_input;
    std::string expected_output;
    std::string tests_json;   // [{"input":"...","expected":"...","label":"..."}]
    bool completed = false;
    std::string user_code;
    std::string last_status;  // "PASS" | "FAIL" | ""
};

struct DsLesson {
    std::string id;           // e.g. "6.1"
    std::string chapter_id;
    int sort_order = 0;
    std::string title;        // e.g. "6.1 单链表原地反转与环检测"
    std::string source_ref;   // e.g. "《算法笔记》7.3 · P253-264"
    std::string time_comp;    // e.g. "O(n)"
    std::string space_comp;   // e.g. "O(1)"
    std::string viz_type;     // "linked_list"|"two_ptr"|"bsearch"|"stack"|"queue"|"tree"|"bst"|"dsu"|"graph"|"dp"|"kmp"|"bit"|"complexity"
    std::string viz_label;
    std::string tutorial_html;
    std::string code_template;
    std::string note;
    bool mastered = false;
    std::vector<DsExercise> exercises;
};

struct DsRunResult {
    bool ok = false;
    bool compiled = false;
    bool all_passed = false;
    std::string compiler_used;
    double compile_ms = 0.0;
    std::string stdout_text;
    std::string stderr_text;
    int passed_count = 0;
    int total_count = 0;
    JsonValue case_results; // array of {label, passed, input, expected, actual, time_ms}
};

/**
 * C++ Algorithms & Data Structures Course Manager:
 *  - Backed by SQLite3 (`omni_ds_course.db`: ds_chapters, ds_lessons, ds_exercises, ds_progress)
 *    with in-memory + JSON persistence fallback when built without SQLite3.
 *  - Provides colorful native web-page tutorials distilled from the PDF/PPTX/outline curriculum,
 *    visualization metadata, personal progress & notes, and live C++17 compilation/grading via g++.
 */
class DsManager {
public:
    explicit DsManager(std::string db_path = "");
    ~DsManager();

    DsManager(const DsManager&) = delete;
    DsManager& operator=(const DsManager&) = delete;

    bool init();

    JsonValue get_catalog(const std::string& query = "", const std::string& category = "") const;
    bool get_lesson(const std::string& lesson_id, DsLesson& out) const;
    JsonValue lesson_to_json(const DsLesson& lesson) const;

    bool save_progress(const std::string& lesson_id,
                       bool mastered,
                       const std::string& note,
                       const std::string& exercise_id = "",
                       const std::string& user_code = "",
                       const std::string& last_status = "",
                       bool mark_completed = false);

    DsRunResult run_and_grade(const std::string& exercise_id,
                              const std::string& user_code,
                              const std::string& custom_input = "",
                              bool run_tests = true);

    int total_lessons() const { return static_cast<int>(lessons_.size()); }
    int completed_exercises() const;
    int total_exercises() const;

    static std::string find_cpp_compiler();

private:
    std::string db_path_;
    void* db_handle_ = nullptr;
    bool sqlite_ready_ = false;

    std::vector<DsChapter> chapters_;
    std::vector<DsLesson> lessons_;

    bool exec_sql(const std::string& sql);
    void seed_curriculum();
    void load_from_sqlite();
    void save_to_sqlite();
    void load_fallback_progress();
    void save_fallback_progress() const;
};

} // namespace omni
