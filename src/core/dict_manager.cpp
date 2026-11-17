#include "dict_manager.hpp"

#include "platform_utils.hpp"
#include "zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>

#ifdef OMNI_USE_SQLITE3
  #include <sqlite3.h>
#endif

namespace omni {

namespace fs = std::filesystem;

namespace {

std::string to_lower_str(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string trim_str(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

#ifdef OMNI_USE_SQLITE3
std::string join_usage(const std::vector<std::string>& u) {
    std::string out;
    for (size_t i = 0; i < u.size(); ++i) {
        if (i > 0) out += "\n";
        out += u[i];
    }
    return out;
}

std::vector<std::string> split_usage(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream iss(s);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) out.push_back(line);
    }
    return out;
}
#endif

std::string normalize_exam_tags(const std::string& raw) {
    std::string low = to_lower_str(raw);
    std::vector<std::string> tags;
    auto add_if = [&](const std::string& needle, const std::string& norm) {
        if (low.find(needle) != std::string::npos) {
            if (std::find(tags.begin(), tags.end(), norm) == tags.end()) tags.push_back(norm);
        }
    };
    add_if("cet4", "CET4");
    add_if("cet6", "CET6");
    add_if("ky", "考研");
    add_if("kaoyan", "考研");
    add_if("考研", "考研");
    add_if("ielts", "IELTS");
    add_if("雅思", "IELTS");
    add_if("toefl", "TOEFL");
    add_if("托福", "TOEFL");
    add_if("level4", "TEM4");
    add_if("tem4", "TEM4");
    add_if("专四", "TEM4");
    add_if("level8", "TEM8");
    add_if("tem8", "TEM8");
    add_if("专八", "TEM8");
    add_if("sat", "SAT");
    add_if("gre", "GRE");
    add_if("gmat", "GMAT");
    add_if("bec", "BEC");
    add_if("gk", "高考");
    add_if("gaozhong", "高中");
    add_if("zk", "中考");
    add_if("chuzhong", "初中");

    if (tags.empty() && !trim_str(raw).empty()) {
        return trim_str(raw);
    }
    std::string res;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i > 0) res += " ";
        res += tags[i];
    }
    return res;
}

std::string infer_tag_from_filename(const std::string& path, const std::string& hint) {
    if (!trim_str(hint).empty()) return normalize_exam_tags(hint);
    std::string fn = path_to_utf8(utf8_to_path(path).stem());
    return normalize_exam_tags(fn);
}

std::string format_ipa(const std::string& raw) {
    std::string t = trim_str(raw);
    if (t.empty()) return "";
    if (t.front() == '/' && t.back() == '/') return t;
    if (t.front() == '[' && t.back() == ']') return t;
    return "/" + t + "/";
}

std::vector<std::string> parse_csv_row(const std::string& line) {
    std::vector<std::string> cols;
    std::string cur;
    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        char c = line[i];
        if (in_quotes) {
            if (c == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    cur.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                cur.push_back(c);
            }
        } else {
            if (c == '"') {
                in_quotes = true;
            } else if (c == ',') {
                cols.push_back(cur);
                cur.clear();
            } else if (c != '\r') {
                cur.push_back(c);
            }
        }
    }
    cols.push_back(cur);
    return cols;
}

std::string replace_literal(std::string s, const std::string& from, const std::string& to) {
    size_t pos = 0;
    while ((pos = s.find(from, pos)) != std::string::npos) {
        s.replace(pos, from.size(), to);
        pos += to.size();
    }
    return s;
}

std::string exchange_code_label(const std::string& code) {
    if (code == "p") return "过去式";
    if (code == "d") return "过去分词";
    if (code == "i") return "现在分词";
    if (code == "3") return "第三人称单数";
    if (code == "s") return "复数";
    if (code == "r") return "比较级";
    if (code == "t") return "最高级";
    if (code == "0") return "原形";
    if (code == "1") return "词形变化";
    return code;
}

bool parse_json_dict_object(const JsonValue& obj, const std::string& fallback_tag, DictEntry& out) {
    if (!obj.is_object()) return false;

    // 1. kajweb/dict format
    if (obj.has("headWord") || (obj.has("content") && obj.get("content").is_object())) {
        out.word = to_lower_str(trim_str(obj.get("headWord").as_string(obj.get("word").as_string())));
        if (out.word.empty()) return false;

        std::string book = obj.get("bookId").as_string();
        out.tags = normalize_exam_tags(book.empty() ? fallback_tag : book);

        const JsonValue& c1 = obj.get("content");
        const JsonValue& c2 = c1.get("word").is_object() ? c1.get("word").get("content") : c1;
        if (c2.is_object()) {
            std::string us = c2.get("usphone").as_string();
            std::string uk = c2.get("ukphone").as_string();
            std::string phone = !us.empty() ? us : (!uk.empty() ? uk : c2.get("phone").as_string());
            out.ipa = format_ipa(phone);
            out.uk_ipa = format_ipa(uk);

            if (c2.get("trans").is_array()) {
                std::ostringstream pos_ss;
                for (size_t i = 0; i < c2.get("trans").arr_val.size(); ++i) {
                    const auto& tr = c2.get("trans").arr_val[i];
                    std::string p = trim_str(tr.get("pos").as_string());
                    std::string cn = trim_str(tr.get("tranCn").as_string());
                    if (cn.empty()) cn = trim_str(tr.get("tranOther").as_string());
                    if (cn.empty()) continue;
                    if (pos_ss.tellp() > 0) pos_ss << "；";
                    if (!p.empty()) pos_ss << p << ". ";
                    pos_ss << cn;
                }
                out.pos = pos_ss.str();
            }

            const JsonValue& sent_arr = c2.get("sentence").get("sentences");
            if (sent_arr.is_array()) {
                for (size_t i = 0; i < sent_arr.arr_val.size() && i < 6; ++i) {
                    std::string en = trim_str(sent_arr.arr_val[i].get("sContent").as_string());
                    std::string zh = trim_str(sent_arr.arr_val[i].get("sCn").as_string());
                    if (!en.empty()) out.sentences.push_back({en, zh, "精选例句"});
                }
            }

            const JsonValue& real_arr = c2.get("realExamSentence").get("sentences");
            if (real_arr.is_array()) {
                for (size_t i = 0; i < real_arr.arr_val.size() && i < 4; ++i) {
                    std::string en = trim_str(real_arr.arr_val[i].get("sContent").as_string());
                    const JsonValue& si = real_arr.arr_val[i].get("sourceInfo");
                    std::string src = trim_str(si.get("level").as_string() + " " + si.get("year").as_string() + " " + si.get("type").as_string());
                    if (!en.empty()) out.sentences.push_back({en, "", src.empty() ? "真题例句" : src});
                }
            }
            if (!out.sentences.empty()) {
                out.example_en = out.sentences[0].en;
                out.example_zh = out.sentences[0].zh;
            }

            const JsonValue& ph_arr = c2.get("phrase").get("phrases");
            if (ph_arr.is_array()) {
                for (size_t i = 0; i < ph_arr.arr_val.size() && i < 8; ++i) {
                    std::string pc = trim_str(ph_arr.arr_val[i].get("pContent").as_string());
                    std::string pcn = trim_str(ph_arr.arr_val[i].get("pCn").as_string());
                    if (!pc.empty()) {
                        out.phrases.push_back({pc, pcn});
                        out.usage.push_back("• " + pc + (pcn.empty() ? "" : "：" + pcn));
                    }
                }
            }

            if (c2.get("remMethod").is_object()) {
                out.rem_method = trim_str(c2.get("remMethod").get("val").as_string());
            }

            const JsonValue& rels = c2.get("relWord").get("rels");
            if (rels.is_array()) {
                std::ostringstream rss;
                for (const auto& r : rels.arr_val) {
                    std::string rpos = trim_str(r.get("pos").as_string());
                    const JsonValue& rw = r.get("words");
                    if (!rw.is_array()) continue;
                    for (size_t i = 0; i < rw.arr_val.size() && i < 3; ++i) {
                        std::string hw = trim_str(rw.arr_val[i].get("hwd").as_string());
                        std::string tr = trim_str(rw.arr_val[i].get("tran").as_string());
                        if (!hw.empty()) {
                            if (rss.tellp() > 0) rss << "；";
                            if (!rpos.empty()) rss << rpos << ". ";
                            rss << hw << (tr.empty() ? "" : " (" + tr + ")");
                        }
                    }
                }
                out.rel_words = rss.str();
            }

            const JsonValue& synos = c2.get("syno").get("synos");
            if (synos.is_array()) {
                std::ostringstream sss;
                for (size_t i = 0; i < synos.arr_val.size() && i < 3; ++i) {
                    const JsonValue& hwds = synos.arr_val[i].get("hwds");
                    if (!hwds.is_array()) continue;
                    for (size_t j = 0; j < hwds.arr_val.size() && j < 4; ++j) {
                        std::string w = trim_str(hwds.arr_val[j].get("w").as_string());
                        if (!w.empty()) {
                            if (sss.tellp() > 0) sss << ", ";
                            sss << w;
                        }
                    }
                }
                out.syno = sss.str();
            }
        }
        return !out.word.empty();
    }

    // 2. KyleBing / flat JSON format
    if (obj.has("word")) {
        out.word = to_lower_str(trim_str(obj.get("word").as_string()));
        if (out.word.empty()) return false;
        out.ipa = format_ipa(obj.get("phonetic").as_string(obj.get("ipa").as_string()));
        out.tags = normalize_exam_tags(obj.get("tag").as_string(obj.get("tags").as_string(fallback_tag)));

        if (obj.get("translations").is_array()) {
            std::ostringstream pos_ss;
            for (size_t i = 0; i < obj.get("translations").arr_val.size(); ++i) {
                const auto& tr = obj.get("translations").arr_val[i];
                std::string t = trim_str(tr.get("type").as_string());
                std::string cn = trim_str(tr.get("translation").as_string());
                if (cn.empty()) continue;
                if (pos_ss.tellp() > 0) pos_ss << "；";
                if (!t.empty()) pos_ss << t << ". ";
                pos_ss << cn;
            }
            out.pos = pos_ss.str();
        } else {
            out.pos = trim_str(obj.get("translation").as_string(obj.get("pos").as_string()));
        }

        if (obj.get("phrases").is_array()) {
            for (size_t i = 0; i < obj.get("phrases").arr_val.size() && i < 6; ++i) {
                const auto& ph = obj.get("phrases").arr_val[i];
                std::string p = trim_str(ph.get("phrase").as_string());
                std::string cn = trim_str(ph.get("translation").as_string());
                if (!p.empty()) {
                    out.phrases.push_back({p, cn});
                    out.usage.push_back("• " + p + (cn.empty() ? "" : "：" + cn));
                }
            }
        }
        out.example_en = trim_str(obj.get("exampleEn").as_string());
        out.example_zh = trim_str(obj.get("exampleZh").as_string());
        if (!out.example_en.empty()) {
            out.sentences.push_back({out.example_en, out.example_zh, "精选例句"});
        }
        return true;
    }

    return false;
}

const std::vector<DictEntry>& starter_entries() {
    static const std::vector<DictEntry> kWords = {
        {"insight", "/ˈɪnsaɪt/", "/ˈɪnsaɪt/", "n. 洞察力；顿悟；深刻见解",
         {"• have / gain an insight into：对……有深刻理解与洞察", "• offer fresh insights into：为……提供全新视角"},
         {{"have / gain an insight into", "对……有深刻理解与洞察"}, {"offer fresh insights into", "为……提供全新视角"}},
         {{"The ablation study provides crucial insight into how cross-attention captures spatial dependencies.", "消融实验为交叉注意力机制如何捕获空间依赖性提供了关键洞察。", "精选例句"}},
         {{"复数", "insights"}}, "s:insights", "in (向内) + sight (看见) → 看透本质，即“洞察力”。", "adj. insightful (有洞察力的)", "perception, intuition, acumen",
         "The ablation study provides crucial insight into how cross-attention captures spatial dependencies.",
         "消融实验为交叉注意力机制如何捕获空间依赖性提供了关键洞察。",
         "- 词根拆解：in (向内) + sight (看见) → 看透本质，即“洞察力”。\n- 写作替换：替换简单的 understanding，提升学术表达质感。",
         "CET6 考研 IELTS TOEFL", "", true, true},
        {"paradigm", "/ˈpærədaɪm/", "/ˈpærədaɪm/", "n. 范式；典范；思维模式",
         {"• paradigm shift：范式转移 / 重大变革", "• dominant paradigm：主流范式"},
         {{"paradigm shift", "范式转移 / 重大变革"}, {"dominant paradigm", "主流范式"}},
         {{"Local-first architecture establishes a new paradigm for personal desktop knowledge tools.", "本地优先架构为个人桌面知识管理工具确立了全新的技术范式。", "精选例句"}},
         {{"复数", "paradigms"}}, "s:paradigms", "para (在旁) + digm (显示) → 展示在旁边的标准样板。", "adj. paradigmatic (典范的)", "model, archetype, pattern",
         "Local-first architecture establishes a new paradigm for personal desktop knowledge tools.",
         "本地优先架构为个人桌面知识管理工具确立了全新的技术范式。",
         "- 常见于论文 Introduction 首段，用于对比传统方法与新方法的代际差异。",
         "CET6 考研 IELTS GRE", "", false, false},
        {"heuristic", "/hjʊəˈrɪstɪk/", "/hjʊəˈrɪstɪk/", "adj. 启发式的；探索法的 n. 启发式算法",
         {"• heuristic search / algorithm：启发式搜索 / 算法", "• rule-of-thumb heuristic：经验法则"},
         {{"heuristic search / algorithm", "启发式搜索 / 算法"}, {"rule-of-thumb heuristic", "经验法则"}},
         {{"We design a lightweight heuristic pruning strategy to accelerate tree traversal.", "我们设计了一种轻量级的启发式剪枝策略以加速树结构遍历。", "精选例句"}},
         {{"复数", "heuristics"}}, "s:heuristics", "", "adv. heuristically (启发式地)", "exploratory, empirical",
         "We design a lightweight heuristic pruning strategy to accelerate tree traversal.",
         "我们设计了一种轻量级的启发式剪枝策略以加速树结构遍历。",
         "- 数学建模与路径规划（如 A* 算法）核心术语。",
         "GRE TOEFL", "", false, false},
        {"stochastic", "/stəˈkæstɪk/", "/stəˈkæstɪk/", "adj. 随机的；概率性的",
         {"• stochastic gradient descent (SGD)：随机梯度下降", "• stochastic process：随机过程"},
         {{"stochastic gradient descent (SGD)", "随机梯度下降"}, {"stochastic process", "随机过程"}},
         {{"Stochastic noise injection improves generalization across unseen domain shifts.", "随机噪声注入提升了模型在未知域偏移下的泛化能力。", "精选例句"}},
         {}, "", "", "adv. stochastically (随机地)", "random, probabilistic",
         "Stochastic noise injection improves generalization across unseen domain shifts.",
         "随机噪声注入提升了模型在未知域偏移下的泛化能力。",
         "- 区分 random（一般随机）与 stochastic（遵循特定概率分布的随机过程）。",
         "GRE", "", false, false},
        {"synthesis", "/ˈsɪnθəsɪs/", "/ˈsɪnθəsɪs/", "n. 综合；合成；整合",
         {"• novel view synthesis：新视角合成", "• in synthesis：总而言之；综合起来"},
         {{"novel view synthesis", "新视角合成"}, {"in synthesis", "总而言之；综合起来"}},
         {{"The workbench achieves a seamless synthesis of Markdown, Word, and LaTeX engines.", "该工作台实现了 Markdown、Word 与 LaTeX 引擎的无缝整合。", "精选例句"}},
         {{"复数", "syntheses"}}, "s:syntheses", "syn (共同) + thesis (放置) → 放到一起合成。", "v. synthesize (合成)；adj. synthetic (合成的)", "integration, fusion",
         "The workbench achieves a seamless synthesis of Markdown, Word, and LaTeX engines.",
         "该工作台实现了 Markdown、Word 与 LaTeX 引擎的无缝整合。",
         "- 动词 synthesize，形容词 synthetic（合成的）。",
         "CET6 考研 IELTS TOEFL", "", false, false},
        {"architecture", "/ˈɑːrkɪtektʃər/", "/ˈɑːkɪtektʃə(r)/", "n. 架构；体系结构；建筑学",
         {"• hybrid architecture：混合架构", "• modular architecture：模块化体系结构"},
         {{"hybrid architecture", "混合架构"}, {"modular architecture", "模块化体系结构"}},
         {{"A decoupled IPC bridge keeps the C++ core engine independent from UI rendering.", "解耦的 IPC 通信桥使 C++ 核心引擎完全独立于界面渲染层。", "精选例句"}},
         {{"复数", "architectures"}}, "s:architectures", "arch (首要) + tect (建造) → 首要建造之学即建筑/架构。", "adj. architectural (建筑的)；n. architect (架构师)", "structure, framework",
         "A decoupled IPC bridge keeps the C++ core engine independent from UI rendering.",
         "解耦的 IPC 通信桥使 C++ 核心引擎完全独立于界面渲染层。",
         "- 系统设计文档高频核心词。",
         "CET4 CET6 考研", "", false, false},
        {"go", "/ɡoʊ/", "/ɡəʊ/", "v. 去；走；运转；变成",
         {"• go through：经历；仔细检查", "• go over：复习；检查"},
         {{"go through", "经历；仔细检查"}, {"go over", "复习；检查"}},
         {{"She went straight to the library after class to review her notes.", "她下课后直接去图书馆复习笔记了。", "精选例句"}},
         {{"过去式", "went"}, {"过去分词", "gone"}, {"现在分词", "going"}, {"第三人称单数", "goes"}},
         "p:went/d:gone/i:going/3:goes", "", "", "proceed, depart, advance",
         "She went straight to the library after class to review her notes.",
         "她下课后直接去图书馆复习笔记了。",
         "- 常见不规则动词：go / went / gone。",
         "CET4", "", false, false}
    };
    return kWords;
}

const std::vector<ReadingArticle>& starter_readings() {
    static const std::vector<ReadingArticle> kReadings = {
        {1, "How Scientific Insight Drives Practical Decisions", "RACE", "reading", "高中 (High)",
         "insight apply heuristic empirical coherent",
         "True scientific insight rarely appears out of thin air. Researchers often apply a simple heuristic when facing complex datasets: test early, observe carefully, and refine every model against empirical evidence.\n\nWhen students learn to write coherent arguments, they apply the same discipline. Rather than memorizing isolated rules, they examine real passages and notice how ideas connect.",
         "真正的科学洞察很少凭空出现。研究者面对复杂数据时常常运用简明的启发式方法：尽早测试、细致观察，并以经验实证不断打磨模型。\n\n当学生学习撰写连贯论述时，他们也应用着同样的原则：不是死记硬背孤立的规则，而是精读真实语篇并体会观点如何衔接。",
         "[{\"question\":\"According to the passage, what do researchers often apply to complex datasets?\",\"options\":[\"A simple heuristic\",\"Random guessing\",\"Memorized formulas\",\"Isolated rules\"],\"answer\":\"A\"},{\"question\":\"How can students learn to write coherent arguments?\",\"options\":[\"By avoiding reading\",\"By memorizing dictionaries\",\"By examining real passages\",\"By skipping evidence\"],\"answer\":\"C\"}]",
         2},
        {2, "A Morning Lesson in Sharing (Cloze Practice)", "CLOTH", "cloze", "初中 (Middle)",
         "apply patience gift",
         "Last Sunday I went to the community library with my younger sister. We wanted to _ our free time to reading stories together. At first, she found the long paragraphs a little _ , so I taught her how to break each sentence into small parts.\n\nBefore we left, she chose a book about space and smiled. Small steps can _ big confidence.",
         "",
         "[{\"options\":[\"apply\",\"waste\",\"forget\",\"hide\"],\"answer\":\"A\"},{\"options\":[\"easy\",\"difficult\",\"bright\",\"loud\"],\"answer\":\"B\"},{\"options\":[\"drop\",\"break\",\"bring\",\"lose\"],\"answer\":\"C\"}]",
         3},
        {3, "TED Bilingual: Small Daily Habits That Compound", "TED", "bilingual", "TED 双语",
         "habit apply progress",
         "Good afternoon, everyone. Today I want to talk about a quiet force that shapes our lives: the small habits we apply every single morning.\n\nIf you improve just one percent each day, that tiny effort does not disappear. Over months, those small gains compound into remarkable progress.",
         "大家下午好。今天我想聊聊一种塑造我们生活的安静力量：每天清晨我们所坚持运用的微小习惯。\n\n如果你每天只进步百分之一，这些微小的付出并不会白费。数月之后，细小的积累就会复利般成长为惊人的进步。",
         "[]",
         0}
    };
    return kReadings;
}

#ifdef OMNI_USE_SQLITE3
std::vector<std::string> tokenize_ascii_alnum(const std::string& text) {
    std::vector<std::string> tokens;
    std::string cur;
    for (unsigned char c : text) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
            cur.push_back(static_cast<char>(std::tolower(c)));
        } else if (!cur.empty()) {
            tokens.push_back(cur);
            cur.clear();
        }
    }
    if (!cur.empty()) tokens.push_back(cur);
    return tokens;
}

int count_json_questions(const std::string& qjson) {
    if (qjson.empty() || qjson == "[]") return 0;
    JsonValue v;
    if (JsonValue::parse(qjson, v) && v.is_array()) {
        return static_cast<int>(v.arr_val.size());
    }
    return 0;
}
#endif

} // namespace

std::vector<DictForm> parse_ecdict_exchange(const std::string& exchange) {
    std::vector<DictForm> forms;
    std::istringstream iss(exchange);
    std::string token;
    while (std::getline(iss, token, '/')) {
        size_t colon = token.find(':');
        if (colon == std::string::npos) continue;
        std::string code = trim_str(token.substr(0, colon));
        std::string val = to_lower_str(trim_str(token.substr(colon + 1)));
        if (code.empty() || val.empty() || code == "0" || code == "1") continue;
        forms.push_back({exchange_code_label(code), val});
    }
    return forms;
}

DictManager::DictManager(std::string db_path) : db_path_(std::move(db_path)) {
    if (db_path_.empty()) {
        db_path_ = path_to_utf8(utf8_to_path(get_app_data_dir()) / "omni_dict.db");
    }
}

DictManager::~DictManager() {
#ifdef OMNI_USE_SQLITE3
    if (db_handle_) {
        sqlite3_close(static_cast<sqlite3*>(db_handle_));
        db_handle_ = nullptr;
    }
#endif
}

bool DictManager::exec_sql(const std::string& sql) {
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

bool DictManager::init() {
#ifdef OMNI_USE_SQLITE3
    std::error_code ec;
    fs::path p = utf8_to_path(db_path_);
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);

    sqlite3* db = nullptr;
    if (sqlite3_open(db_path_.c_str(), &db) == SQLITE_OK) {
        db_handle_ = db;
        sqlite_ready_ = true;
        // 1. 核心词条表
        exec_sql(
            "CREATE TABLE IF NOT EXISTS words ("
            "  word TEXT PRIMARY KEY,"
            "  ipa TEXT,"
            "  uk_ipa TEXT DEFAULT '',"
            "  pos TEXT,"
            "  usage TEXT,"
            "  example_en TEXT,"
            "  example_zh TEXT,"
            "  note TEXT,"
            "  tags TEXT DEFAULT '',"
            "  exchange TEXT DEFAULT '',"
            "  rem_method TEXT DEFAULT '',"
            "  rel_words TEXT DEFAULT '',"
            "  syno TEXT DEFAULT '',"
            "  marked INTEGER DEFAULT 0,"
            "  favorite INTEGER DEFAULT 0"
            ");"
        );
        // 安全迁移旧表字段
        exec_sql("ALTER TABLE words ADD COLUMN tags TEXT DEFAULT '';");
        exec_sql("ALTER TABLE words ADD COLUMN uk_ipa TEXT DEFAULT '';");
        exec_sql("ALTER TABLE words ADD COLUMN exchange TEXT DEFAULT '';");
        exec_sql("ALTER TABLE words ADD COLUMN rem_method TEXT DEFAULT '';");
        exec_sql("ALTER TABLE words ADD COLUMN rel_words TEXT DEFAULT '';");
        exec_sql("ALTER TABLE words ADD COLUMN syno TEXT DEFAULT '';");
        exec_sql("CREATE INDEX IF NOT EXISTS idx_words_tags ON words(tags);");

        // 2. 固定搭配短语表
        exec_sql(
            "CREATE TABLE IF NOT EXISTS word_phrases ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  word TEXT NOT NULL,"
            "  phrase TEXT NOT NULL,"
            "  translation TEXT DEFAULT '',"
            "  UNIQUE(word, phrase)"
            ");"
        );
        exec_sql("CREATE INDEX IF NOT EXISTS idx_phrases_word ON word_phrases(word);");

        // 3. 双语与真题例句表
        exec_sql(
            "CREATE TABLE IF NOT EXISTS word_sentences ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  word TEXT NOT NULL,"
            "  en TEXT NOT NULL,"
            "  zh TEXT DEFAULT '',"
            "  source TEXT DEFAULT '精选例句',"
            "  UNIQUE(word, en)"
            ");"
        );
        exec_sql("CREATE INDEX IF NOT EXISTS idx_sentences_word ON word_sentences(word);");

        // 4. 词形变化反向索引表 (例如 went -> go)
        exec_sql(
            "CREATE TABLE IF NOT EXISTS word_forms ("
            "  inflected TEXT NOT NULL,"
            "  lemma TEXT NOT NULL,"
            "  form_label TEXT DEFAULT '',"
            "  PRIMARY KEY(inflected, lemma)"
            ");"
        );
        exec_sql("CREATE INDEX IF NOT EXISTS idx_forms_inflected ON word_forms(inflected);");

        // 5. 用户学习笔记表 (独立于词库表，重导词库绝不覆盖)
        exec_sql(
            "CREATE TABLE IF NOT EXISTS word_notes ("
            "  word TEXT PRIMARY KEY,"
            "  note TEXT DEFAULT '',"
            "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP"
            ");"
        );

        // 6. 长文应用与真题题目表 (RACE / CLOTH / BBC / TED / SimpleWiki / TinyStories / Gutenberg)
        exec_sql(
            "CREATE TABLE IF NOT EXISTS reading_articles ("
            "  id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "  title TEXT NOT NULL,"
            "  source TEXT DEFAULT '',"
            "  kind TEXT DEFAULT 'reading',"
            "  level TEXT DEFAULT '',"
            "  target_words TEXT DEFAULT '',"
            "  passage_en TEXT DEFAULT '',"
            "  passage_zh TEXT DEFAULT '',"
            "  questions_json TEXT DEFAULT '[]',"
            "  created_at DATETIME DEFAULT CURRENT_TIMESTAMP"
            ");"
        );
        exec_sql("ALTER TABLE reading_articles ADD COLUMN source TEXT DEFAULT '';");
        exec_sql("ALTER TABLE reading_articles ADD COLUMN kind TEXT DEFAULT 'reading';");
        exec_sql("ALTER TABLE reading_articles ADD COLUMN question_count INTEGER DEFAULT 0;");
        exec_sql("CREATE INDEX IF NOT EXISTS idx_reading_source ON reading_articles(source);");
        exec_sql("CREATE INDEX IF NOT EXISTS idx_reading_kind ON reading_articles(kind);");

        // 检查或建立 FTS5 全文检索索引，以便查词秒级检索所有含该词的文章与真题
        reading_fts_ready_ = exec_sql(
            "CREATE VIRTUAL TABLE IF NOT EXISTS reading_fts USING fts5("
            "  title, passage_en, content='reading_articles', content_rowid='id'"
            ");"
        );

        // 7. 文章单词颜色标记表 (持久化每篇文章中被用户高光标记的生词、颜色、句意与上下文)
        exec_sql(
            "CREATE TABLE IF NOT EXISTS reading_word_marks ("
            "  article_id INTEGER NOT NULL,"
            "  word TEXT NOT NULL,"
            "  surface TEXT DEFAULT '',"
            "  color TEXT NOT NULL,"
            "  meaning TEXT DEFAULT '',"
            "  context TEXT DEFAULT '',"
            "  updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,"
            "  PRIMARY KEY(article_id, word)"
            ");"
        );
        exec_sql("CREATE INDEX IF NOT EXISTS idx_rwm_article ON reading_word_marks(article_id);");

        seed_initial_words();
        return true;
    }
#endif
    fallback_store_ = starter_entries();
    fallback_readings_ = starter_readings();
    return true;
}

void DictManager::seed_initial_words() {
#ifdef OMNI_USE_SQLITE3
    if (!sqlite_ready_) return;
    if (total_count() == 0) {
        exec_sql("BEGIN TRANSACTION;");
        for (const auto& e : starter_entries()) {
            upsert_word_internal(e);
        }
        exec_sql("COMMIT;");
    }
    if (reading_count() == 0) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        const char* ins = "INSERT INTO reading_articles "
                          "(title, source, kind, level, target_words, passage_en, passage_zh, questions_json, question_count) "
                          "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
        exec_sql("BEGIN TRANSACTION;");
        for (const auto& r : starter_readings()) {
            sqlite3_stmt* st = nullptr;
            if (sqlite3_prepare_v2(db, ins, -1, &st, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(st, 1, r.title.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 2, r.source.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 3, r.kind.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 4, r.level.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 5, r.target_words.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 6, r.passage_en.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 7, r.passage_zh.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(st, 8, r.questions_json.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(st, 9, r.question_count);
                sqlite3_step(st);
                sqlite3_finalize(st);
            }
        }
        exec_sql("COMMIT;");
        if (reading_fts_ready_) {
            exec_sql("INSERT INTO reading_fts(reading_fts) VALUES('rebuild');");
        }
    }
#endif
}

int DictManager::total_count() const {
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        sqlite3_stmt* stmt = nullptr;
        int count = 0;
        if (sqlite3_prepare_v2(db, "SELECT COUNT(*) FROM words;", -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        return count;
    }
#endif
    return static_cast<int>(fallback_store_.size());
}

void DictManager::populate_child_tables(DictEntry& e) const {
    if (!e.exchange.empty() && e.forms.empty()) {
        e.forms = parse_ecdict_exchange(e.exchange);
    }
#ifdef OMNI_USE_SQLITE3
    if (!sqlite_ready_) return;
    sqlite3* db = static_cast<sqlite3*>(db_handle_);

    // 1. 查询搭配短语表
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT phrase, translation FROM word_phrases WHERE word = ? ORDER BY id ASC LIMIT 10;", -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, e.word.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char* c0 = sqlite3_column_text(st, 0);
                const unsigned char* c1 = sqlite3_column_text(st, 1);
                std::string p = c0 ? reinterpret_cast<const char*>(c0) : "";
                std::string tr = c1 ? reinterpret_cast<const char*>(c1) : "";
                if (!p.empty()) e.phrases.push_back({p, tr});
            }
            sqlite3_finalize(st);
        }
    }

    // 2. 查询双语例句表
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT en, zh, source FROM word_sentences WHERE word = ? ORDER BY id ASC LIMIT 10;", -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, e.word.c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char* c0 = sqlite3_column_text(st, 0);
                const unsigned char* c1 = sqlite3_column_text(st, 1);
                const unsigned char* c2 = sqlite3_column_text(st, 2);
                std::string en = c0 ? reinterpret_cast<const char*>(c0) : "";
                std::string zh = c1 ? reinterpret_cast<const char*>(c1) : "";
                std::string src = c2 ? reinterpret_cast<const char*>(c2) : "";
                if (!en.empty()) e.sentences.push_back({en, zh, src});
            }
            sqlite3_finalize(st);
        }
        if (e.sentences.empty() && !e.example_en.empty()) {
            e.sentences.push_back({e.example_en, e.example_zh, "精选例句"});
        }
    }

    // 3. 查询独立学习笔记表
    {
        sqlite3_stmt* st = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT note FROM word_notes WHERE word = ? LIMIT 1;", -1, &st, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(st, 1, e.word.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(st) == SQLITE_ROW) {
                const unsigned char* c0 = sqlite3_column_text(st, 0);
                std::string n = c0 ? reinterpret_cast<const char*>(c0) : "";
                if (!n.empty()) e.note = n;
            }
            sqlite3_finalize(st);
        }
    }
#endif
}

bool DictManager::upsert_word_internal(const DictEntry& entry) {
    std::string key = to_lower_str(entry.word);
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        const char* sql =
            "INSERT INTO words (word, ipa, uk_ipa, pos, usage, example_en, example_zh, note, tags, exchange, rem_method, rel_words, syno, marked, favorite) "
            "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?) "
            "ON CONFLICT(word) DO UPDATE SET "
            "ipa=CASE WHEN excluded.ipa!='' THEN excluded.ipa ELSE words.ipa END, "
            "uk_ipa=CASE WHEN excluded.uk_ipa!='' THEN excluded.uk_ipa ELSE words.uk_ipa END, "
            "pos=CASE WHEN excluded.pos!='' THEN excluded.pos ELSE words.pos END, "
            "usage=CASE WHEN excluded.usage!='' THEN excluded.usage ELSE words.usage END, "
            "example_en=CASE WHEN excluded.example_en!='' THEN excluded.example_en ELSE words.example_en END, "
            "example_zh=CASE WHEN excluded.example_zh!='' THEN excluded.example_zh ELSE words.example_zh END, "
            "exchange=CASE WHEN excluded.exchange!='' THEN excluded.exchange ELSE words.exchange END, "
            "rem_method=CASE WHEN excluded.rem_method!='' THEN excluded.rem_method ELSE words.rem_method END, "
            "rel_words=CASE WHEN excluded.rel_words!='' THEN excluded.rel_words ELSE words.rel_words END, "
            "syno=CASE WHEN excluded.syno!='' THEN excluded.syno ELSE words.syno END, "
            "tags=CASE WHEN words.tags='' THEN excluded.tags "
            "          WHEN excluded.tags!='' AND instr(words.tags, excluded.tags)=0 THEN words.tags || ' ' || excluded.tags "
            "          ELSE words.tags END;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
        std::string u_str = join_usage(entry.usage);
        sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, entry.ipa.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, entry.uk_ipa.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 4, entry.pos.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 5, u_str.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 6, entry.example_en.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 7, entry.example_zh.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 8, entry.note.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 9, entry.tags.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 10, entry.exchange.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 11, entry.rem_method.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 12, entry.rel_words.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 13, entry.syno.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 14, entry.marked ? 1 : 0);
        sqlite3_bind_int(stmt, 15, entry.favorite ? 1 : 0);
        bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);

        if (ok) {
            if (!entry.phrases.empty()) {
                sqlite3_stmt* pst = nullptr;
                if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO word_phrases (word, phrase, translation) VALUES (?, ?, ?);", -1, &pst, nullptr) == SQLITE_OK) {
                    for (const auto& ph : entry.phrases) {
                        sqlite3_reset(pst);
                        sqlite3_bind_text(pst, 1, key.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(pst, 2, ph.phrase.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(pst, 3, ph.translation.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_step(pst);
                    }
                    sqlite3_finalize(pst);
                }
            }
            if (!entry.sentences.empty()) {
                sqlite3_stmt* sst = nullptr;
                if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO word_sentences (word, en, zh, source) VALUES (?, ?, ?, ?);", -1, &sst, nullptr) == SQLITE_OK) {
                    for (const auto& sn : entry.sentences) {
                        sqlite3_reset(sst);
                        sqlite3_bind_text(sst, 1, key.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(sst, 2, sn.en.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(sst, 3, sn.zh.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_bind_text(sst, 4, sn.source.c_str(), -1, SQLITE_TRANSIENT);
                        sqlite3_step(sst);
                    }
                    sqlite3_finalize(sst);
                }
            }
            if (!entry.exchange.empty()) {
                auto forms = parse_ecdict_exchange(entry.exchange);
                if (!forms.empty()) {
                    sqlite3_stmt* fst = nullptr;
                    if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO word_forms (inflected, lemma, form_label) VALUES (?, ?, ?);", -1, &fst, nullptr) == SQLITE_OK) {
                        for (const auto& fm : forms) {
                            sqlite3_reset(fst);
                            sqlite3_bind_text(fst, 1, fm.word.c_str(), -1, SQLITE_TRANSIENT);
                            sqlite3_bind_text(fst, 2, key.c_str(), -1, SQLITE_TRANSIENT);
                            sqlite3_bind_text(fst, 3, fm.form_label.c_str(), -1, SQLITE_TRANSIENT);
                            sqlite3_step(fst);
                        }
                        sqlite3_finalize(fst);
                    }
                }
            }
            if (!entry.note.empty()) {
                sqlite3_stmt* nst = nullptr;
                if (sqlite3_prepare_v2(db, "INSERT OR IGNORE INTO word_notes (word, note) VALUES (?, ?);", -1, &nst, nullptr) == SQLITE_OK) {
                    sqlite3_bind_text(nst, 1, key.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_bind_text(nst, 2, entry.note.c_str(), -1, SQLITE_TRANSIENT);
                    sqlite3_step(nst);
                    sqlite3_finalize(nst);
                }
            }
        }
        return ok;
    }
#endif
    for (auto& w : fallback_store_) {
        if (to_lower_str(w.word) == key) {
            if (!entry.ipa.empty()) w.ipa = entry.ipa;
            if (!entry.uk_ipa.empty()) w.uk_ipa = entry.uk_ipa;
            if (!entry.pos.empty()) w.pos = entry.pos;
            if (!entry.usage.empty()) w.usage = entry.usage;
            if (!entry.phrases.empty()) w.phrases = entry.phrases;
            if (!entry.sentences.empty()) w.sentences = entry.sentences;
            if (!entry.exchange.empty()) {
                w.exchange = entry.exchange;
                w.forms = parse_ecdict_exchange(entry.exchange);
            }
            if (!entry.rem_method.empty()) w.rem_method = entry.rem_method;
            if (!entry.rel_words.empty()) w.rel_words = entry.rel_words;
            if (!entry.syno.empty()) w.syno = entry.syno;
            if (!entry.example_en.empty()) w.example_en = entry.example_en;
            if (!entry.example_zh.empty()) w.example_zh = entry.example_zh;
            if (!entry.tags.empty() && w.tags.find(entry.tags) == std::string::npos) {
                if (!w.tags.empty()) w.tags += " ";
                w.tags += entry.tags;
            }
            return true;
        }
    }
    DictEntry copy = entry;
    copy.word = key;
    if (!copy.exchange.empty() && copy.forms.empty()) {
        copy.forms = parse_ecdict_exchange(copy.exchange);
    }
    fallback_store_.push_back(std::move(copy));
    return true;
}

bool DictManager::upsert_word(const DictEntry& entry) {
    return upsert_word_internal(entry);
}

static bool is_ascii_word_query(const std::string& q) {
    if (q.empty()) return false;
    for (unsigned char c : q) {
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '\'' || c == '_' || c == ' ')) {
            return false;
        }
    }
    return true;
}

int DictManager::count_matches(const std::string& query,
                               const std::string& tag_filter,
                               bool marked_only) const {
    std::string q = to_lower_str(trim_str(query));
    std::string tf = trim_str(tag_filter);
    bool ascii_q = is_ascii_word_query(q);
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        std::ostringstream sql;
        sql << "SELECT COUNT(*) FROM words WHERE 1=1 ";
        if (!q.empty()) {
            sql << (ascii_q ? "AND word LIKE ? " : "AND (word LIKE ? OR pos LIKE ?) ");
        }
        if (!tf.empty()) sql << "AND tags LIKE ? ";
        if (marked_only) sql << "AND marked = 1 ";
        sql << ";";

        sqlite3_stmt* stmt = nullptr;
        int cnt = 0;
        if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            std::string contains = "%" + q + "%";
            std::string tag_pat = "%" + tf + "%";
            if (!q.empty()) {
                sqlite3_bind_text(stmt, idx++, contains.c_str(), -1, SQLITE_TRANSIENT);
                if (!ascii_q) sqlite3_bind_text(stmt, idx++, contains.c_str(), -1, SQLITE_TRANSIENT);
            }
            if (!tf.empty()) {
                sqlite3_bind_text(stmt, idx++, tag_pat.c_str(), -1, SQLITE_TRANSIENT);
            }
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                cnt = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
        return cnt;
    }
#endif
    int cnt = 0;
    for (const auto& e : fallback_store_) {
        if (marked_only && !e.marked) continue;
        if (!tf.empty() && to_lower_str(e.tags).find(to_lower_str(tf)) == std::string::npos) continue;
        if (q.empty() || to_lower_str(e.word).find(q) != std::string::npos ||
            (!ascii_q && to_lower_str(e.pos).find(q) != std::string::npos)) {
            ++cnt;
        }
    }
    return cnt;
}

std::vector<DictEntry> DictManager::search(const std::string& query,
                                           int limit,
                                           const std::string& tag_filter,
                                           bool marked_only,
                                           bool include_details,
                                           int offset) const {
    std::string q = to_lower_str(trim_str(query));
    std::string tf = trim_str(tag_filter);
    bool ascii_q = is_ascii_word_query(q);
    if (offset < 0) offset = 0;
    std::vector<DictEntry> out;
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);

        // 首页才检查词形变化反向索引（例如搜 went 自动匹配到原形 go）
        if (!q.empty() && offset == 0) {
            sqlite3_stmt* fst = nullptr;
            if (sqlite3_prepare_v2(db, "SELECT lemma, form_label FROM word_forms WHERE inflected = ? LIMIT 3;", -1, &fst, nullptr) == SQLITE_OK) {
                sqlite3_bind_text(fst, 1, q.c_str(), -1, SQLITE_TRANSIENT);
                while (sqlite3_step(fst) == SQLITE_ROW) {
                    const unsigned char* c0 = sqlite3_column_text(fst, 0);
                    const unsigned char* c1 = sqlite3_column_text(fst, 1);
                    std::string lemma = c0 ? reinterpret_cast<const char*>(c0) : "";
                    std::string flabel = c1 ? reinterpret_cast<const char*>(c1) : "";
                    DictEntry le;
                    if (!lemma.empty() && lemma != q && get_word(lemma, le)) {
                        le.matched_from = q + " (" + flabel + ")";
                        out.push_back(std::move(le));
                    }
                }
                sqlite3_finalize(fst);
            }
        }

        std::ostringstream sql;
        sql << "SELECT word, ipa, uk_ipa, pos, usage, example_en, example_zh, note, tags, exchange, rem_method, rel_words, syno, marked, favorite FROM words WHERE 1=1 ";
        if (!q.empty()) {
            sql << (ascii_q ? "AND word LIKE ? " : "AND (word LIKE ? OR pos LIKE ?) ");
        }
        if (!tf.empty()) sql << "AND tags LIKE ? ";
        if (marked_only) sql << "AND marked = 1 ";
        sql << "ORDER BY ";
        if (!q.empty()) {
            // 优先匹配精确/同前缀且拼写长度最相近的词（形近优先），不再让无关已标记词插队
            sql << "CASE WHEN word = ? THEN 0 WHEN word LIKE ? THEN 1 WHEN word LIKE ? THEN 2 ELSE 3 END, "
                   "ABS(length(word) - ?), length(word) ASC, word ASC LIMIT ? OFFSET ?;";
        } else {
            sql << "marked DESC, word ASC LIMIT ? OFFSET ?;";
        }

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            std::string contains = "%" + q + "%";
            std::string prefix = q + "%";
            std::string tag_pat = "%" + tf + "%";
            if (!q.empty()) {
                sqlite3_bind_text(stmt, idx++, contains.c_str(), -1, SQLITE_TRANSIENT);
                if (!ascii_q) sqlite3_bind_text(stmt, idx++, contains.c_str(), -1, SQLITE_TRANSIENT);
            }
            if (!tf.empty()) {
                sqlite3_bind_text(stmt, idx++, tag_pat.c_str(), -1, SQLITE_TRANSIENT);
            }
            if (!q.empty()) {
                sqlite3_bind_text(stmt, idx++, q.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, prefix.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, contains.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, idx++, static_cast<int>(q.size()));
            }
            sqlite3_bind_int(stmt, idx++, limit);
            sqlite3_bind_int(stmt, idx++, offset);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int c) -> std::string {
                    const unsigned char* t = sqlite3_column_text(stmt, c);
                    return t ? reinterpret_cast<const char*>(t) : "";
                };
                std::string w = col(0);
                bool dup = false;
                for (const auto& existing : out) {
                    if (existing.word == w) { dup = true; break; }
                }
                if (dup) continue;

                DictEntry e;
                e.word = std::move(w);
                e.ipa = col(1);
                e.uk_ipa = col(2);
                e.pos = col(3);
                e.usage = split_usage(col(4));
                e.example_en = col(5);
                e.example_zh = col(6);
                e.note = col(7);
                e.tags = col(8);
                e.exchange = col(9);
                e.rem_method = col(10);
                e.rel_words = col(11);
                e.syno = col(12);
                e.marked = sqlite3_column_int(stmt, 13) != 0;
                e.favorite = sqlite3_column_int(stmt, 14) != 0;
                if (include_details || out.empty()) {
                    populate_child_tables(e);
                }
                out.push_back(std::move(e));
                if (static_cast<int>(out.size()) >= limit) break;
            }
            sqlite3_finalize(stmt);
        }
        return out;
    }
#endif
    // Fallback (non-SQLite3)
    std::vector<DictEntry> all;
    if (!q.empty()) {
        for (const auto& e : fallback_store_) {
            for (const auto& fm : e.forms) {
                if (fm.word == q && e.word != q) {
                    DictEntry copy = e;
                    copy.matched_from = q + " (" + fm.form_label + ")";
                    all.push_back(std::move(copy));
                    break;
                }
            }
        }
    }
    (void)include_details;
    for (const auto& e : fallback_store_) {
        if (marked_only && !e.marked) continue;
        if (!tf.empty() && to_lower_str(e.tags).find(to_lower_str(tf)) == std::string::npos) continue;
        if (q.empty() || to_lower_str(e.word).find(q) != std::string::npos ||
            (!ascii_q && to_lower_str(e.pos).find(q) != std::string::npos)) {
            bool dup = false;
            for (const auto& x : all) if (x.word == e.word) { dup = true; break; }
            if (!dup) all.push_back(e);
        }
    }
    for (int i = offset; i < static_cast<int>(all.size()) && static_cast<int>(out.size()) < limit; ++i) {
        out.push_back(all[i]);
    }
    return out;
}

static int levenshtein_dist(const std::string& a, const std::string& b) {
    const size_t n = a.size(), m = b.size();
    std::vector<int> prev(m + 1), cur(m + 1);
    for (size_t j = 0; j <= m; ++j) prev[j] = static_cast<int>(j);
    for (size_t i = 1; i <= n; ++i) {
        cur[0] = static_cast<int>(i);
        for (size_t j = 1; j <= m; ++j) {
            int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
            cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + cost});
        }
        std::swap(prev, cur);
    }
    return prev[m];
}

static int ortho_similarity_score(const std::string& target, const std::string& cand) {
    int dist = levenshtein_dist(target, cand);
    int lcp = 0;
    size_t min_len = std::min(target.size(), cand.size());
    while (lcp < static_cast<int>(min_len) && target[lcp] == cand[lcp]) ++lcp;
    int lcs = 0;
    while (lcs < static_cast<int>(min_len) &&
           target[target.size() - 1 - lcs] == cand[cand.size() - 1 - lcs]) {
        ++lcs;
    }
    int len_diff = std::abs(static_cast<int>(target.size()) - static_cast<int>(cand.size()));
    return dist * 10 - lcp * 4 - lcs * 3 + len_diff;
}

std::vector<DictEntry> DictManager::similar_words(const std::string& anchor_word,
                                                  const std::string& tag_filter,
                                                  int limit,
                                                  int offset,
                                                  int* out_total) const {
    std::string target = to_lower_str(trim_str(anchor_word));
    std::string tf = trim_str(tag_filter);
    if (offset < 0) offset = 0;
    std::vector<DictEntry> out;
    if (target.empty()) {
        if (out_total) *out_total = 0;
        return out;
    }

    struct Scored {
        int score;
        DictEntry entry;
    };
    std::vector<Scored> ranked;

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        std::string p2 = (target.size() >= 2 ? target.substr(0, 2) : target) + "%";
        std::string p1 = target.substr(0, 1) + "%";
        std::string s2 = "%" + (target.size() >= 2 ? target.substr(target.size() - 2) : target);
        std::string mid = target.size() >= 4 ? ("%" + target.substr(1, std::min<size_t>(3, target.size() - 2)) + "%") : ("%" + target + "%");
        int len_min = std::max(1, static_cast<int>(target.size()) - 2);
        int len_max = static_cast<int>(target.size()) + 4;
        std::string tag_pat = "%" + tf + "%";

        std::ostringstream sql;
        sql << "SELECT word, ipa, uk_ipa, pos, usage, example_en, example_zh, note, tags, exchange, rem_method, rel_words, syno, marked, favorite "
               "FROM words WHERE word != ? AND (word LIKE ? OR (word LIKE ? AND length(word) BETWEEN ? AND ?) OR word LIKE ? OR word LIKE ?) ";
        if (!tf.empty()) sql << "AND tags LIKE ? ";
        sql << "LIMIT 800;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            sqlite3_bind_text(stmt, idx++, target.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, p2.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, p1.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, idx++, len_min);
            sqlite3_bind_int(stmt, idx++, len_max);
            sqlite3_bind_text(stmt, idx++, s2.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, idx++, mid.c_str(), -1, SQLITE_TRANSIENT);
            if (!tf.empty()) sqlite3_bind_text(stmt, idx++, tag_pat.c_str(), -1, SQLITE_TRANSIENT);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int c) -> std::string {
                    const unsigned char* t = sqlite3_column_text(stmt, c);
                    return t ? reinterpret_cast<const char*>(t) : "";
                };
                DictEntry e;
                e.word = col(0);
                e.ipa = col(1);
                e.uk_ipa = col(2);
                e.pos = col(3);
                e.usage = split_usage(col(4));
                e.example_en = col(5);
                e.example_zh = col(6);
                e.note = col(7);
                e.tags = col(8);
                e.exchange = col(9);
                e.rem_method = col(10);
                e.rel_words = col(11);
                e.syno = col(12);
                e.marked = sqlite3_column_int(stmt, 13) != 0;
                e.favorite = sqlite3_column_int(stmt, 14) != 0;
                int sc = ortho_similarity_score(target, e.word);
                ranked.push_back({sc, std::move(e)});
            }
            sqlite3_finalize(stmt);
        }
    } else
#endif
    {
        for (const auto& e : fallback_store_) {
            if (e.word == target) continue;
            if (!tf.empty() && to_lower_str(e.tags).find(to_lower_str(tf)) == std::string::npos) continue;
            ranked.push_back({ortho_similarity_score(target, to_lower_str(e.word)), e});
        }
    }

    std::sort(ranked.begin(), ranked.end(), [](const Scored& a, const Scored& b) {
        if (a.score != b.score) return a.score < b.score;
        return a.entry.word < b.entry.word;
    });

    if (out_total) *out_total = static_cast<int>(ranked.size());
    for (int i = offset; i < static_cast<int>(ranked.size()) && static_cast<int>(out.size()) < limit; ++i) {
        DictEntry item = std::move(ranked[i].entry);
#ifdef OMNI_USE_SQLITE3
        if (sqlite_ready_ && out.empty()) populate_child_tables(item);
#endif
        out.push_back(std::move(item));
    }
    return out;
}

int DictManager::count_by_phonetic(const std::string& combo,
                                   const std::string& ipa_fragment) const {
    std::string c_low = to_lower_str(trim_str(combo));
    std::string ipa_clean = trim_str(ipa_fragment);
    if (ipa_clean.size() >= 2 && ipa_clean.front() == '/' && ipa_clean.back() == '/') {
        ipa_clean = ipa_clean.substr(1, ipa_clean.size() - 2);
    }
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        std::ostringstream sql;
        sql << "SELECT COUNT(*) FROM words WHERE ";
        if (!c_low.empty() && !ipa_clean.empty()) {
            sql << "(word LIKE ? AND ipa LIKE ?) OR ipa LIKE ? OR word LIKE ? ";
        } else if (!c_low.empty()) {
            sql << "word LIKE ? ";
        } else if (!ipa_clean.empty()) {
            sql << "ipa LIKE ? ";
        } else {
            sql << "1=1 ";
        }
        sql << ";";
        sqlite3_stmt* stmt = nullptr;
        int cnt = 0;
        if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            std::string c_pat = "%" + c_low + "%";
            std::string i_pat = "%" + ipa_clean + "%";
            if (!c_low.empty() && !ipa_clean.empty()) {
                sqlite3_bind_text(stmt, idx++, c_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, c_pat.c_str(), -1, SQLITE_TRANSIENT);
            } else if (!c_low.empty()) {
                sqlite3_bind_text(stmt, idx++, c_pat.c_str(), -1, SQLITE_TRANSIENT);
            } else if (!ipa_clean.empty()) {
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
            }
            if (sqlite3_step(stmt) == SQLITE_ROW) cnt = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        return cnt;
    }
#endif
    int cnt = 0;
    for (const auto& e : fallback_store_) {
        bool has_combo = !c_low.empty() && to_lower_str(e.word).find(c_low) != std::string::npos;
        bool has_ipa = !ipa_clean.empty() && e.ipa.find(ipa_clean) != std::string::npos;
        if (has_combo || has_ipa || (c_low.empty() && ipa_clean.empty())) ++cnt;
    }
    return cnt;
}

std::vector<DictEntry> DictManager::search_by_phonetic(const std::string& combo,
                                                       const std::string& ipa_fragment,
                                                       int limit,
                                                       int offset) const {
    std::string c_low = to_lower_str(trim_str(combo));
    std::string ipa_clean = trim_str(ipa_fragment);
    if (ipa_clean.size() >= 2 && ipa_clean.front() == '/' && ipa_clean.back() == '/') {
        ipa_clean = ipa_clean.substr(1, ipa_clean.size() - 2);
    }
    if (offset < 0) offset = 0;
    std::vector<DictEntry> out;

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        std::ostringstream sql;
        sql << "SELECT word, ipa, uk_ipa, pos, usage, example_en, example_zh, note, tags, exchange, rem_method, rel_words, syno, marked, favorite FROM words WHERE ";
        if (!c_low.empty() && !ipa_clean.empty()) {
            sql << "(word LIKE ? AND ipa LIKE ?) OR ipa LIKE ? OR word LIKE ? ";
        } else if (!c_low.empty()) {
            sql << "word LIKE ? ";
        } else if (!ipa_clean.empty()) {
            sql << "ipa LIKE ? ";
        } else {
            sql << "1=1 ";
        }
        sql << "ORDER BY ";
        if (!c_low.empty() && !ipa_clean.empty()) {
            sql << "CASE WHEN (word LIKE ? AND ipa LIKE ?) THEN 0 WHEN ipa LIKE ? THEN 1 ELSE 2 END, ";
        }
        sql << "length(word) ASC, word ASC LIMIT ? OFFSET ?;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            std::string c_pat = "%" + c_low + "%";
            std::string i_pat = "%" + ipa_clean + "%";
            if (!c_low.empty() && !ipa_clean.empty()) {
                sqlite3_bind_text(stmt, idx++, c_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, c_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, c_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
            } else if (!c_low.empty()) {
                sqlite3_bind_text(stmt, idx++, c_pat.c_str(), -1, SQLITE_TRANSIENT);
            } else if (!ipa_clean.empty()) {
                sqlite3_bind_text(stmt, idx++, i_pat.c_str(), -1, SQLITE_TRANSIENT);
            }
            sqlite3_bind_int(stmt, idx++, limit);
            sqlite3_bind_int(stmt, idx++, offset);

            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int c) -> std::string {
                    const unsigned char* t = sqlite3_column_text(stmt, c);
                    return t ? reinterpret_cast<const char*>(t) : "";
                };
                DictEntry e;
                e.word = col(0);
                e.ipa = col(1);
                e.uk_ipa = col(2);
                e.pos = col(3);
                e.usage = split_usage(col(4));
                e.example_en = col(5);
                e.example_zh = col(6);
                e.note = col(7);
                e.tags = col(8);
                e.exchange = col(9);
                e.rem_method = col(10);
                e.rel_words = col(11);
                e.syno = col(12);
                e.marked = sqlite3_column_int(stmt, 13) != 0;
                e.favorite = sqlite3_column_int(stmt, 14) != 0;
                if (out.empty()) populate_child_tables(e);
                out.push_back(std::move(e));
            }
            sqlite3_finalize(stmt);
        }
        return out;
    }
#endif
    std::vector<DictEntry> primary, secondary, merged;
    for (const auto& e : fallback_store_) {
        bool has_combo = !c_low.empty() && to_lower_str(e.word).find(c_low) != std::string::npos;
        bool has_ipa = !ipa_clean.empty() && e.ipa.find(ipa_clean) != std::string::npos;
        if ((!c_low.empty() && !ipa_clean.empty() && has_combo && has_ipa) ||
            (c_low.empty() && has_ipa) ||
            (ipa_clean.empty() && has_combo)) {
            primary.push_back(e);
        } else if (has_combo || has_ipa) {
            secondary.push_back(e);
        }
    }
    merged.reserve(primary.size() + secondary.size());
    for (auto& e : primary) merged.push_back(std::move(e));
    for (auto& e : secondary) merged.push_back(std::move(e));
    for (int i = offset; i < static_cast<int>(merged.size()) && static_cast<int>(out.size()) < limit; ++i) {
        out.push_back(std::move(merged[i]));
    }
    return out;
}

bool DictManager::get_word(const std::string& word, DictEntry& out) const {
    std::string key = to_lower_str(trim_str(word));
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        const char* sql = "SELECT word, ipa, uk_ipa, pos, usage, example_en, example_zh, note, tags, exchange, rem_method, rel_words, syno, marked, favorite FROM words WHERE word = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        bool found = false;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int idx) -> std::string {
                    const unsigned char* t = sqlite3_column_text(stmt, idx);
                    return t ? reinterpret_cast<const char*>(t) : "";
                };
                out.word = col(0);
                out.ipa = col(1);
                out.uk_ipa = col(2);
                out.pos = col(3);
                out.usage = split_usage(col(4));
                out.example_en = col(5);
                out.example_zh = col(6);
                out.note = col(7);
                out.tags = col(8);
                out.exchange = col(9);
                out.rem_method = col(10);
                out.rel_words = col(11);
                out.syno = col(12);
                out.marked = sqlite3_column_int(stmt, 13) != 0;
                out.favorite = sqlite3_column_int(stmt, 14) != 0;
                populate_child_tables(out);
                found = true;
            }
            sqlite3_finalize(stmt);
        }
        if (found) return true;

        // 若直接查不到，尝试通过 word_forms 词形还原表查找
        sqlite3_stmt* fst = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT lemma, form_label FROM word_forms WHERE inflected = ? LIMIT 1;", -1, &fst, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(fst, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(fst) == SQLITE_ROW) {
                const unsigned char* c0 = sqlite3_column_text(fst, 0);
                const unsigned char* c1 = sqlite3_column_text(fst, 1);
                std::string lemma = c0 ? reinterpret_cast<const char*>(c0) : "";
                std::string flabel = c1 ? reinterpret_cast<const char*>(c1) : "";
                sqlite3_finalize(fst);
                if (!lemma.empty() && lemma != key && get_word(lemma, out)) {
                    out.matched_from = key + " (" + flabel + ")";
                    return true;
                }
            } else {
                sqlite3_finalize(fst);
            }
        }
        return false;
    }
#endif
    for (const auto& e : fallback_store_) {
        if (to_lower_str(e.word) == key) {
            out = e;
            return true;
        }
    }
    for (const auto& e : fallback_store_) {
        for (const auto& fm : e.forms) {
            if (fm.word == key) {
                out = e;
                out.matched_from = key + " (" + fm.form_label + ")";
                return true;
            }
        }
    }
    return false;
}

bool DictManager::set_marked(const std::string& word, bool marked) {
    std::string key = to_lower_str(trim_str(word));
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "UPDATE words SET marked = ? WHERE word = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, marked ? 1 : 0);
            sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
            bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);
            return ok;
        }
        return false;
    }
#endif
    for (auto& e : fallback_store_) {
        if (to_lower_str(e.word) == key) { e.marked = marked; return true; }
    }
    return false;
}

bool DictManager::set_favorite(const std::string& word, bool favorite) {
    std::string key = to_lower_str(trim_str(word));
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, "UPDATE words SET favorite = ? WHERE word = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, favorite ? 1 : 0);
            sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
            bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);
            return ok;
        }
        return false;
    }
#endif
    for (auto& e : fallback_store_) {
        if (to_lower_str(e.word) == key) { e.favorite = favorite; return true; }
    }
    return false;
}

bool DictManager::save_note(const std::string& word, const std::string& note) {
    std::string key = to_lower_str(trim_str(word));
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        sqlite3_stmt* stmt = nullptr;
        bool ok = false;
        if (sqlite3_prepare_v2(db,
            "INSERT INTO word_notes (word, note, updated_at) VALUES (?, ?, CURRENT_TIMESTAMP) "
            "ON CONFLICT(word) DO UPDATE SET note=excluded.note, updated_at=CURRENT_TIMESTAMP;",
            -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 2, note.c_str(), -1, SQLITE_TRANSIENT);
            ok = (sqlite3_step(stmt) == SQLITE_DONE);
            sqlite3_finalize(stmt);
        }
        sqlite3_stmt* wstmt = nullptr;
        if (sqlite3_prepare_v2(db, "UPDATE words SET note = ? WHERE word = ?;", -1, &wstmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(wstmt, 1, note.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(wstmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_step(wstmt);
            sqlite3_finalize(wstmt);
        }
        return ok;
    }
#endif
    for (auto& e : fallback_store_) {
        if (to_lower_str(e.word) == key) { e.note = note; return true; }
    }
    return false;
}

bool DictManager::import_dictionary_file(const std::string& file_path,
                                         const std::string& book_tag_hint,
                                         int& out_imported,
                                         std::string* err) {
    out_imported = 0;
    std::ifstream ifs(utf8_to_path(file_path), std::ios::binary);
    if (!ifs) {
        if (err) *err = "Cannot open dictionary file: " + file_path;
        return false;
    }

    std::string fallback_tag = infer_tag_from_filename(file_path, book_tag_hint);
    std::string ext = to_lower_str(path_to_utf8(utf8_to_path(file_path).extension()));

    std::string raw_json_override;
    if (ext == ".zip") {
        ZipArchive zip;
        if (!zip.load_from_file(file_path, err)) return false;
        for (const auto& name : zip.list_entries()) {
            std::string low = to_lower_str(name);
            if (low.size() >= 5 && low.compare(low.size() - 5, 5, ".json") == 0) {
                zip.get_entry(name, raw_json_override);
                break;
            }
        }
        if (raw_json_override.empty()) {
            if (err) *err = "No .json entry found inside zip: " + file_path;
            return false;
        }
        ext = ".json";
    }

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) exec_sql("BEGIN TRANSACTION;");
#endif

    if (ext == ".csv") {
        std::string line;
        bool first_row = true;
        int col_word = 0, col_phonetic = 1, col_def = 2, col_trans = 3, col_tag = 7, col_exchange = 10;

        while (std::getline(ifs, line)) {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (first_row && line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF) {
                line = line.substr(3);
            }
            if (trim_str(line).empty()) continue;

            auto cols = parse_csv_row(line);
            if (first_row) {
                first_row = false;
                if (!cols.empty() && to_lower_str(trim_str(cols[0])) == "word") {
                    for (size_t i = 0; i < cols.size(); ++i) {
                        std::string h = to_lower_str(trim_str(cols[i]));
                        if (h == "word") col_word = static_cast<int>(i);
                        else if (h == "phonetic" || h == "ipa") col_phonetic = static_cast<int>(i);
                        else if (h == "definition") col_def = static_cast<int>(i);
                        else if (h == "translation" || h == "meaning") col_trans = static_cast<int>(i);
                        else if (h == "tag" || h == "tags") col_tag = static_cast<int>(i);
                        else if (h == "exchange") col_exchange = static_cast<int>(i);
                    }
                    continue;
                }
            }

            if (static_cast<int>(cols.size()) <= col_word) continue;
            DictEntry e;
            e.word = to_lower_str(trim_str(cols[col_word]));
            if (e.word.empty()) continue;
            if (col_phonetic < static_cast<int>(cols.size())) e.ipa = format_ipa(cols[col_phonetic]);
            std::string trans = (col_trans < static_cast<int>(cols.size())) ? trim_str(cols[col_trans]) : "";
            std::string def = (col_def < static_cast<int>(cols.size())) ? trim_str(cols[col_def]) : "";
            trans = replace_literal(trans, "\\n", "；");
            def = replace_literal(def, "\\n", "；");
            e.pos = !trans.empty() ? trans : def;
            if (!def.empty() && !trans.empty()) {
                e.usage.push_back("• EN: " + def);
            }
            if (col_exchange < static_cast<int>(cols.size())) {
                e.exchange = trim_str(cols[col_exchange]);
            }

            std::string raw_tag = (col_tag < static_cast<int>(cols.size())) ? cols[col_tag] : "";
            e.tags = normalize_exam_tags(raw_tag.empty() ? fallback_tag : raw_tag);

            if (upsert_word_internal(e)) ++out_imported;
        }
    } else {
        std::string raw = std::move(raw_json_override);
        if (raw.empty()) {
            std::ostringstream buf;
            buf << ifs.rdbuf();
            raw = buf.str();
        }
        if (raw.size() >= 3 &&
            static_cast<unsigned char>(raw[0]) == 0xEF &&
            static_cast<unsigned char>(raw[1]) == 0xBB &&
            static_cast<unsigned char>(raw[2]) == 0xBF) {
            raw = raw.substr(3);
        }

        std::string t = trim_str(raw);
        if (!t.empty() && t.front() == '[') {
            JsonValue arr;
            std::string parse_err;
            if (!JsonValue::parse(t, arr, &parse_err) || !arr.is_array()) {
#ifdef OMNI_USE_SQLITE3
                if (sqlite_ready_) exec_sql("ROLLBACK;");
#endif
                if (err) *err = "Invalid JSON dictionary array: " + parse_err;
                return false;
            }
            for (const auto& item : arr.arr_val) {
                DictEntry e;
                if (parse_json_dict_object(item, fallback_tag, e)) {
                    if (upsert_word_internal(e)) ++out_imported;
                }
            }
        } else {
            std::istringstream iss(raw);
            std::string line;
            while (std::getline(iss, line)) {
                std::string lt = trim_str(line);
                if (lt.empty()) continue;
                JsonValue obj;
                if (JsonValue::parse(lt, obj) && obj.is_object()) {
                    DictEntry e;
                    if (parse_json_dict_object(obj, fallback_tag, e)) {
                        if (upsert_word_internal(e)) ++out_imported;
                    }
                }
            }
        }
    }

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) exec_sql("COMMIT;");
#endif
    return out_imported > 0;
}

JsonValue DictManager::entry_to_json(const DictEntry& e) const {
    JsonValue obj = JsonValue::make_object();
    obj["word"] = e.word;
    obj["ipa"] = e.ipa;
    obj["ukIpa"] = e.uk_ipa;
    obj["pos"] = e.pos;

    JsonValue u = JsonValue::make_array();
    for (const auto& s : e.usage) u.push_back(s);
    obj["usage"] = std::move(u);

    JsonValue ph = JsonValue::make_array();
    for (const auto& p : e.phrases) {
        JsonValue item = JsonValue::make_object();
        item["phrase"] = p.phrase;
        item["translation"] = p.translation;
        ph.push_back(std::move(item));
    }
    obj["phrases"] = std::move(ph);

    JsonValue sents = JsonValue::make_array();
    for (const auto& sn : e.sentences) {
        JsonValue item = JsonValue::make_object();
        item["en"] = sn.en;
        item["zh"] = sn.zh;
        item["source"] = sn.source;
        sents.push_back(std::move(item));
    }
    obj["sentences"] = std::move(sents);

    JsonValue fms = JsonValue::make_array();
    for (const auto& fm : e.forms) {
        JsonValue item = JsonValue::make_object();
        item["label"] = fm.form_label;
        item["word"] = fm.word;
        fms.push_back(std::move(item));
    }
    obj["forms"] = std::move(fms);

    obj["remMethod"] = e.rem_method;
    obj["relWords"] = e.rel_words;
    obj["syno"] = e.syno;
    obj["exampleEn"] = e.example_en;
    obj["exampleZh"] = e.example_zh;
    obj["note"] = e.note;
    obj["tags"] = e.tags;
    obj["matchedFrom"] = e.matched_from;
    obj["marked"] = e.marked;
    obj["favorite"] = e.favorite;
    return obj;
}

int DictManager::reading_count() const {
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3_stmt* stmt = nullptr;
        int cnt = 0;
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_handle_), "SELECT COUNT(*) FROM reading_articles;", -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) cnt = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        return cnt;
    }
#endif
    return static_cast<int>(fallback_readings_.size());
}

int DictManager::total_reading_questions() const {
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3_stmt* stmt = nullptr;
        int cnt = 0;
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_handle_), "SELECT COALESCE(SUM(question_count), 0) FROM reading_articles;", -1, &stmt, nullptr) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) cnt = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        return cnt;
    }
#endif
    int sum = 0;
    for (const auto& r : fallback_readings_) sum += r.question_count;
    return sum;
}

#ifdef OMNI_USE_SQLITE3
static std::string build_reading_fts_expr(sqlite3* db, const std::vector<std::string>& tokens) {
    if (tokens.empty()) return "";
    if (tokens.size() == 1) {
        std::vector<std::string> forms = {tokens[0]};
        sqlite3_stmt* fst = nullptr;
        if (sqlite3_prepare_v2(db, "SELECT DISTINCT inflected FROM word_forms WHERE lemma = ? LIMIT 8;", -1, &fst, nullptr) == SQLITE_OK) {
            sqlite3_bind_text(fst, 1, tokens[0].c_str(), -1, SQLITE_TRANSIENT);
            while (sqlite3_step(fst) == SQLITE_ROW) {
                const unsigned char* c0 = sqlite3_column_text(fst, 0);
                if (c0) {
                    std::string f = to_lower_str(reinterpret_cast<const char*>(c0));
                    if (!f.empty() && std::find(forms.begin(), forms.end(), f) == forms.end()) {
                        forms.push_back(std::move(f));
                    }
                }
            }
            sqlite3_finalize(fst);
        }
        std::string expr;
        for (size_t i = 0; i < forms.size(); ++i) {
            if (i > 0) expr += " OR ";
            expr += "\"" + forms[i] + "\"";
        }
        return expr;
    }
    std::string expr;
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (i > 0) expr += " AND ";
        expr += "\"" + tokens[i] + "\"";
    }
    return expr;
}
#endif

void DictManager::reading_stats(const std::string& query,
                                const std::string& source,
                                const std::string& kind,
                                int& out_articles,
                                int& out_questions) const {
    out_articles = 0;
    out_questions = 0;
    std::string q_low = to_lower_str(trim_str(query));
    std::string src = trim_str(source);
    std::string knd = trim_str(kind);

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        std::vector<std::string> tokens = tokenize_ascii_alnum(q_low);
        if (!tokens.empty() && reading_fts_ready_) {
            std::string fts_expr = build_reading_fts_expr(db, tokens);
            std::ostringstream sql;
            sql << "SELECT COUNT(*), COALESCE(SUM(r.question_count), 0) "
                   "FROM reading_articles r JOIN reading_fts f ON r.id = f.rowid "
                   "WHERE reading_fts MATCH ? ";
            if (!src.empty()) sql << "AND r.source = ? ";
            if (!knd.empty()) sql << "AND r.kind = ? ";
            sql << ";";
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                int idx = 1;
                sqlite3_bind_text(stmt, idx++, fts_expr.c_str(), -1, SQLITE_TRANSIENT);
                if (!src.empty()) sqlite3_bind_text(stmt, idx++, src.c_str(), -1, SQLITE_TRANSIENT);
                if (!knd.empty()) sqlite3_bind_text(stmt, idx++, knd.c_str(), -1, SQLITE_TRANSIENT);
                if (sqlite3_step(stmt) == SQLITE_ROW) {
                    out_articles = sqlite3_column_int(stmt, 0);
                    out_questions = sqlite3_column_int(stmt, 1);
                }
                sqlite3_finalize(stmt);
                if (out_articles > 0) return;
            }
        }
        std::ostringstream sql;
        sql << "SELECT COUNT(*), COALESCE(SUM(question_count), 0) FROM reading_articles WHERE 1=1 ";
        if (!q_low.empty()) sql << "AND (title LIKE ? OR passage_en LIKE ? OR passage_zh LIKE ?) ";
        if (!src.empty()) sql << "AND source = ? ";
        if (!knd.empty()) sql << "AND kind = ? ";
        sql << ";";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            std::string pat = "%" + q_low + "%";
            if (!q_low.empty()) {
                sqlite3_bind_text(stmt, idx++, pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, pat.c_str(), -1, SQLITE_TRANSIENT);
            }
            if (!src.empty()) sqlite3_bind_text(stmt, idx++, src.c_str(), -1, SQLITE_TRANSIENT);
            if (!knd.empty()) sqlite3_bind_text(stmt, idx++, knd.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                out_articles = sqlite3_column_int(stmt, 0);
                out_questions = sqlite3_column_int(stmt, 1);
            }
            sqlite3_finalize(stmt);
        }
        return;
    }
#endif
    for (const auto& r : fallback_readings_) {
        if (!src.empty() && r.source != src) continue;
        if (!knd.empty() && r.kind != knd) continue;
        if (!q_low.empty()) {
            std::string hay = to_lower_str(r.title + " " + r.passage_en + " " + r.passage_zh + " " + r.target_words);
            if (hay.find(q_low) == std::string::npos) continue;
        }
        ++out_articles;
        out_questions += r.question_count;
    }
}

std::vector<ReadingArticle> DictManager::search_readings(const std::string& query,
                                                         const std::string& source,
                                                         const std::string& kind,
                                                         int limit,
                                                         int offset) const {
    std::string q = trim_str(query);
    std::string q_low = to_lower_str(q);
    std::string src = trim_str(source);
    std::string knd = trim_str(kind);
    if (offset < 0) offset = 0;
    std::vector<ReadingArticle> out;

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        std::vector<std::string> tokens = tokenize_ascii_alnum(q_low);

        // 若有英文单词查询且 FTS5 可用，先通过 word_forms 扩展词形并利用 FTS5 秒级匹配
        if (!tokens.empty() && reading_fts_ready_) {
            std::string fts_expr = build_reading_fts_expr(db, tokens);

            std::ostringstream sql;
            sql << "SELECT r.id, r.title, r.source, r.kind, r.level, r.target_words, "
                   "substr(r.passage_en, 1, 240), '', r.questions_json, r.question_count "
                   "FROM reading_articles r "
                   "JOIN reading_fts f ON r.id = f.rowid "
                   "WHERE reading_fts MATCH ? ";
            if (!src.empty()) sql << "AND r.source = ? ";
            if (!knd.empty()) sql << "AND r.kind = ? ";
            sql << "ORDER BY bm25(reading_fts), r.id ASC LIMIT ? OFFSET ?;";

            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                int idx = 1;
                sqlite3_bind_text(stmt, idx++, fts_expr.c_str(), -1, SQLITE_TRANSIENT);
                if (!src.empty()) sqlite3_bind_text(stmt, idx++, src.c_str(), -1, SQLITE_TRANSIENT);
                if (!knd.empty()) sqlite3_bind_text(stmt, idx++, knd.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_int(stmt, idx++, limit);
                sqlite3_bind_int(stmt, idx++, offset);
                while (sqlite3_step(stmt) == SQLITE_ROW) {
                    auto col = [&](int c) -> std::string {
                        const unsigned char* t = sqlite3_column_text(stmt, c);
                        return t ? reinterpret_cast<const char*>(t) : "";
                    };
                    ReadingArticle r;
                    r.id = sqlite3_column_int(stmt, 0);
                    r.title = col(1);
                    r.source = col(2);
                    r.kind = col(3);
                    r.level = col(4);
                    r.target_words = col(5);
                    r.passage_en = col(6);
                    r.passage_zh = col(7);
                    r.questions_json = col(8);
                    int qc = sqlite3_column_int(stmt, 9);
                    r.question_count = (qc > 0) ? qc : count_json_questions(r.questions_json);
                    out.push_back(std::move(r));
                }
                sqlite3_finalize(stmt);
                if (!out.empty()) return out;
            }
        }

        // 无查询词 / 中文查询 / FTS 未建或无结果时的常规筛选查询
        std::ostringstream sql;
        sql << "SELECT id, title, source, kind, level, target_words, "
               "substr(passage_en, 1, 240), '', questions_json, question_count "
               "FROM reading_articles WHERE 1=1 ";
        if (!q_low.empty()) sql << "AND (title LIKE ? OR passage_en LIKE ? OR passage_zh LIKE ?) ";
        if (!src.empty()) sql << "AND source = ? ";
        if (!knd.empty()) sql << "AND kind = ? ";
        sql << "ORDER BY CASE source "
               "WHEN 'Kaoyan' THEN 0 WHEN 'CET6' THEN 1 WHEN 'CET4' THEN 2 "
               "WHEN 'LSAT' THEN 3 WHEN 'ReClor' THEN 4 WHEN 'QuALITY' THEN 5 "
               "ELSE 6 END, id ASC LIMIT ? OFFSET ?;";

        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db, sql.str().c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
            int idx = 1;
            std::string pat = "%" + q_low + "%";
            if (!q_low.empty()) {
                sqlite3_bind_text(stmt, idx++, pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, pat.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_bind_text(stmt, idx++, pat.c_str(), -1, SQLITE_TRANSIENT);
            }
            if (!src.empty()) sqlite3_bind_text(stmt, idx++, src.c_str(), -1, SQLITE_TRANSIENT);
            if (!knd.empty()) sqlite3_bind_text(stmt, idx++, knd.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_int(stmt, idx++, limit);
            sqlite3_bind_int(stmt, idx++, offset);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int c) -> std::string {
                    const unsigned char* t = sqlite3_column_text(stmt, c);
                    return t ? reinterpret_cast<const char*>(t) : "";
                };
                ReadingArticle r;
                r.id = sqlite3_column_int(stmt, 0);
                r.title = col(1);
                r.source = col(2);
                r.kind = col(3);
                r.level = col(4);
                r.target_words = col(5);
                r.passage_en = col(6);
                r.passage_zh = col(7);
                r.questions_json = col(8);
                int qc = sqlite3_column_int(stmt, 9);
                r.question_count = (qc > 0) ? qc : count_json_questions(r.questions_json);
                out.push_back(std::move(r));
            }
            sqlite3_finalize(stmt);
        }
        return out;
    }
#endif
    std::vector<ReadingArticle> matched;
    for (const auto& r : fallback_readings_) {
        if (!src.empty() && r.source != src) continue;
        if (!knd.empty() && r.kind != knd) continue;
        if (!q_low.empty()) {
            std::string hay = to_lower_str(r.title + " " + r.passage_en + " " + r.passage_zh + " " + r.target_words);
            if (hay.find(q_low) == std::string::npos) continue;
        }
        matched.push_back(r);
    }
    for (int i = offset; i < static_cast<int>(matched.size()) && static_cast<int>(out.size()) < limit; ++i) {
        out.push_back(matched[i]);
    }
    return out;
}

bool DictManager::get_reading(int id, ReadingArticle& out) const {
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3_stmt* stmt = nullptr;
        bool found = false;
        const char* sql = "SELECT id, title, source, kind, level, target_words, passage_en, passage_zh, questions_json "
                          "FROM reading_articles WHERE id = ? LIMIT 1;";
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_handle_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, id);
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int c) -> std::string {
                    const unsigned char* t = sqlite3_column_text(stmt, c);
                    return t ? reinterpret_cast<const char*>(t) : "";
                };
                out.id = sqlite3_column_int(stmt, 0);
                out.title = col(1);
                out.source = col(2);
                out.kind = col(3);
                out.level = col(4);
                out.target_words = col(5);
                out.passage_en = col(6);
                out.passage_zh = col(7);
                out.questions_json = col(8);
                out.question_count = count_json_questions(out.questions_json);
                found = true;
            }
            sqlite3_finalize(stmt);
        }
        return found;
    }
#endif
    for (const auto& r : fallback_readings_) {
        if (r.id == id) {
            out = r;
            return true;
        }
    }
    return false;
}

JsonValue DictManager::reading_to_json(const ReadingArticle& r, bool include_body) const {
    JsonValue obj = JsonValue::make_object();
    obj["id"] = r.id;
    obj["title"] = r.title;
    obj["source"] = r.source;
    obj["kind"] = r.kind;
    obj["level"] = r.level;
    obj["targetWords"] = r.target_words;
    obj["questionCount"] = r.question_count;
    if (include_body) {
        obj["passageEn"] = r.passage_en;
        obj["passageZh"] = r.passage_zh;
        JsonValue qs = JsonValue::make_array();
        if (!r.questions_json.empty()) {
            JsonValue parsed;
            if (JsonValue::parse(r.questions_json, parsed) && parsed.is_array()) {
                qs = std::move(parsed);
            }
        }
        obj["questions"] = std::move(qs);
    } else {
        obj["snippet"] = r.passage_en;
    }
    return obj;
}

std::vector<ReadingWordMark> DictManager::get_reading_marks(int article_id) const {
    std::vector<ReadingWordMark> out;
#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "SELECT article_id, word, surface, color, meaning, context "
                          "FROM reading_word_marks WHERE article_id = ? ORDER BY updated_at DESC;";
        if (sqlite3_prepare_v2(static_cast<sqlite3*>(db_handle_), sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, article_id);
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                auto col = [&](int c) -> std::string {
                    const unsigned char* t = sqlite3_column_text(stmt, c);
                    return t ? reinterpret_cast<const char*>(t) : "";
                };
                ReadingWordMark m;
                m.article_id = sqlite3_column_int(stmt, 0);
                m.word = col(1);
                m.surface = col(2);
                m.color = col(3);
                m.meaning = col(4);
                m.context = col(5);
                out.push_back(std::move(m));
            }
            sqlite3_finalize(stmt);
        }
        return out;
    }
#endif
    for (const auto& m : fallback_marks_) {
        if (m.article_id == article_id) out.push_back(m);
    }
    return out;
}

bool DictManager::set_reading_mark(const ReadingWordMark& mark) {
    std::string key = to_lower_str(trim_str(mark.word));
    std::string color = to_lower_str(trim_str(mark.color));
    if (mark.article_id <= 0 || key.empty()) return false;

#ifdef OMNI_USE_SQLITE3
    if (sqlite_ready_) {
        sqlite3* db = static_cast<sqlite3*>(db_handle_);
        if (color.empty()) {
            sqlite3_stmt* stmt = nullptr;
            if (sqlite3_prepare_v2(db, "DELETE FROM reading_word_marks WHERE article_id = ? AND word = ?;", -1, &stmt, nullptr) == SQLITE_OK) {
                sqlite3_bind_int(stmt, 1, mark.article_id);
                sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                return true;
            }
            return false;
        }
        sqlite3_stmt* stmt = nullptr;
        const char* sql = "INSERT INTO reading_word_marks (article_id, word, surface, color, meaning, context, updated_at) "
                          "VALUES (?, ?, ?, ?, ?, ?, CURRENT_TIMESTAMP) "
                          "ON CONFLICT(article_id, word) DO UPDATE SET "
                          "surface=excluded.surface, color=excluded.color, meaning=excluded.meaning, "
                          "context=excluded.context, updated_at=CURRENT_TIMESTAMP;";
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int(stmt, 1, mark.article_id);
            sqlite3_bind_text(stmt, 2, key.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 3, mark.surface.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 4, color.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, mark.meaning.c_str(), -1, SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 6, mark.context.c_str(), -1, SQLITE_TRANSIENT);
            bool ok = sqlite3_step(stmt) == SQLITE_DONE;
            sqlite3_finalize(stmt);
            return ok;
        }
        return false;
    }
#endif
    for (auto it = fallback_marks_.begin(); it != fallback_marks_.end(); ++it) {
        if (it->article_id == mark.article_id && it->word == key) {
            if (color.empty()) {
                fallback_marks_.erase(it);
                return true;
            }
            *it = mark;
            it->word = key;
            it->color = color;
            return true;
        }
    }
    if (!color.empty()) {
        ReadingWordMark copy = mark;
        copy.word = key;
        copy.color = color;
        fallback_marks_.push_back(std::move(copy));
    }
    return true;
}

JsonValue DictManager::marks_to_json(const std::vector<ReadingWordMark>& marks) const {
    JsonValue arr = JsonValue::make_array();
    for (const auto& m : marks) {
        JsonValue obj = JsonValue::make_object();
        obj["articleId"] = m.article_id;
        obj["word"] = m.word;
        obj["surface"] = m.surface;
        obj["color"] = m.color;
        obj["meaning"] = m.meaning;
        obj["context"] = m.context;
        arr.push_back(std::move(obj));
    }
    return arr;
}

} // namespace omni
