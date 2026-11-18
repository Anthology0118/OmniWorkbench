#pragma once

#include "ds_manager.hpp"

#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace omni {
namespace ds_seed {

inline std::string html_esc(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

inline std::string make_tut(
    const std::string& lead,
    const std::vector<std::pair<std::string, std::string>>& steps,
    const std::string& extra_html,
    const std::string& pitfall) {
    std::ostringstream oss;
    oss << "<div class=\"ds-tut-lead\">" << lead << "</div>\n";
    if (!steps.empty()) {
        oss << "<div class=\"ds-tut-steps\">\n";
        const char* tones[] = {"blue", "green", "purple", "amber"};
        const char* badges[] = {"①", "②", "③", "④"};
        for (size_t i = 0; i < steps.size(); ++i) {
            oss << "  <div class=\"ds-step-card tone-" << tones[i % 4] << "\">"
                << "<div class=\"ds-step-title\"><span class=\"ds-step-num\">" << badges[i % 4] << "</span>"
                << steps[i].first << "</div>"
                << "<div class=\"ds-step-desc\">" << steps[i].second << "</div></div>\n";
        }
        oss << "</div>\n";
    }
    if (!extra_html.empty()) {
        oss << extra_html << "\n";
    }
    if (!pitfall.empty()) {
        oss << "<div class=\"ds-pitfall-box\"><strong>⚠ 边界与避坑：</strong>" << pitfall << "</div>\n";
    }
    return oss.str();
}

inline void add_ex(
    DsLesson& les,
    const std::string& id,
    const std::string& title,
    const std::string& mode,
    const std::string& prompt,
    const std::string& sample_in,
    const std::string& expected_out,
    const std::string& tests_json,
    const std::string& starter,
    const std::string& solution) {
    DsExercise ex;
    ex.id = id;
    ex.lesson_id = les.id;
    ex.sort_order = static_cast<int>(les.exercises.size()) + 1;
    ex.title = title;
    ex.mode = mode;
    ex.prompt = prompt;
    ex.sample_input = sample_in;
    ex.expected_output = expected_out;
    ex.tests_json = tests_json;
    ex.starter_code = starter;
    ex.solution_code = solution;
    ex.user_code = starter;
    les.exercises.push_back(std::move(ex));
}

} // namespace ds_seed
} // namespace omni
