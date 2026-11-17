#include "vault_manager.hpp"

#include "docx_converter.hpp"
#include "markdown.hpp"
#include "pdf_converter.hpp"
#include "platform_utils.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <functional>
#include <sstream>

namespace omni {

namespace fs = std::filesystem;

namespace {

std::string lower_ext(const fs::path& p) {
    std::string ext = path_to_utf8(p.extension());
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

bool is_supported_doc_ext(const std::string& ext) {
    return ext == ".md" || ext == ".markdown" || ext == ".docx" || ext == ".txt" || ext == ".tex";
}

bool is_hidden_name(const std::string& name) {
    return !name.empty() && name[0] == '.';
}

struct DirEntrySort {
    bool is_dir;
    std::string name;
    fs::path path;
};

std::vector<DirEntrySort> list_sorted_children(const fs::path& dir) {
    std::vector<DirEntrySort> res;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        std::string name = path_to_utf8(entry.path().filename());
        if (is_hidden_name(name)) continue;
        bool is_d = entry.is_directory(ec);
        if (!is_d && !is_supported_doc_ext(lower_ext(entry.path()))) continue;
        res.push_back({is_d, name, entry.path()});
    }
    std::sort(res.begin(), res.end(), [](const DirEntrySort& a, const DirEntrySort& b) {
        if (a.is_dir != b.is_dir) return a.is_dir > b.is_dir;
        return a.name < b.name;
    });
    return res;
}

std::string utf8_safe_truncate(const std::string& s, size_t max_bytes) {
    if (s.size() <= max_bytes) return s;
    size_t i = max_bytes;
    while (i > 0 && (static_cast<unsigned char>(s[i]) & 0xC0) == 0x80) --i;
    return s.substr(0, i) + "...";
}

std::string clean_preview_line(const std::string& raw) {
    std::string no_tags;
    bool in_tag = false;
    for (char ch : raw) {
        if (ch == '<') { in_tag = true; continue; }
        if (ch == '>') { in_tag = false; continue; }
        if (!in_tag && ch != '*' && ch != '`' && ch != '[' && ch != ']' && ch != '~') {
            no_tags.push_back(ch);
        }
    }
    size_t a = 0;
    while (a < no_tags.size() && std::isspace(static_cast<unsigned char>(no_tags[a]))) ++a;
    size_t b = no_tags.size();
    while (b > a && std::isspace(static_cast<unsigned char>(no_tags[b - 1]))) --b;
    return no_tags.substr(a, b - a);
}

void extract_doc_preview(const fs::path& fp, const std::string& ext, std::string& out_title, std::string& out_snippet) {
    out_title.clear();
    out_snippet.clear();
    std::string text;
    if (ext == ".docx") {
        docx_to_markdown(path_to_utf8(fp), text);
    } else {
        std::ifstream ifs(fp, std::ios::binary);
        if (ifs) {
            text.assign(768, '\0');
            ifs.read(&text[0], static_cast<std::streamsize>(text.size()));
            text.resize(static_cast<size_t>(ifs.gcount()));
        }
    }

    std::istringstream iss(text);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t a = 0;
        while (a < line.size() && std::isspace(static_cast<unsigned char>(line[a]))) ++a;
        std::string t = line.substr(a);
        if (t.empty() || t.rfind("<!--", 0) == 0 || t.rfind("```", 0) == 0 || t == "$$") continue;
        if (t[0] == '#' && out_title.empty()) {
            size_t p = 0;
            while (p < t.size() && (t[p] == '#' || t[p] == ' ')) ++p;
            out_title = utf8_safe_truncate(clean_preview_line(t.substr(p)), 42);
        } else {
            std::string clean = clean_preview_line(t);
            if (!clean.empty()) {
                if (!out_snippet.empty()) out_snippet += " ";
                out_snippet += clean;
                if (out_snippet.size() >= 110) break;
            }
        }
    }
    if (out_snippet.empty() && ext == ".docx") {
        out_snippet = "Word (.docx) 富文本文档 · 点击打开并同步编辑";
    } else {
        out_snippet = utf8_safe_truncate(out_snippet, 110);
    }
}

} // namespace

VaultManager::VaultManager(std::string root_path) : root_path_(std::move(root_path)) {
    if (root_path_.empty()) {
        root_path_ = get_default_vault_dir();
    }
}

void VaultManager::set_root(const std::string& root_path) {
    if (!root_path.empty()) {
        root_path_ = root_path;
    }
}

void VaultManager::ensure_initialized() {
    std::error_code ec;
    fs::path root_p = utf8_to_path(root_path_);
    if (!fs::exists(root_p, ec)) {
        fs::create_directories(root_p, ec);
    }

    bool empty = true;
    for (const auto& entry : fs::directory_iterator(root_p, ec)) {
        std::string name = path_to_utf8(entry.path().filename());
        if (!is_hidden_name(name)) {
            empty = false;
            break;
        }
    }

    if (empty) {
        std::string plan_md =
            "# 2026 年项目研发与数学模型规划\n\n"
            "原生支持 **Markdown** ↔ **Word (.docx)** 双向转换与 **PDF** 导出。\n\n"
            "## 核心目标函数\n\n"
            "$$f(x) = \\int_{-\\infty}^{\\infty} e^{-t^2} dt = \\sqrt{\\pi}$$\n\n"
            "## 关联笔记与表格\n\n"
            "- 英语生词沉淀：[[Vocabulary]]\n"
            "- 公式推导笔记：[[公式笔记]]\n\n"
            "| 季度 | 收敛精度 | 算力预算 (万元) |\n"
            "| :--- | :---: | :---: |\n"
            "| Q1 | 78.4% | 12.0 |\n"
            "| Q2 | 84.9% | 15.5 |\n"
            "| Q3 | 91.2% | 18.0 |\n"
            "| Q4 | 96.5% | 22.0 |\n";

        write_file_utf8(path_to_utf8(root_p / utf8_to_path("数学建模/模型推导/2026核心项目规划.md")), plan_md);
        // Also generate a real .docx alongside it to demonstrate native Word editing & round-trip!
        markdown_to_docx(plan_md, path_to_utf8(root_p / utf8_to_path("数学建模/模型推导/2026核心项目规划.docx")));

        std::string formula_md =
            "# 公式笔记 (LaTeX & OMML)\n\n"
            "支持 LaTeX 数学块自动转为 Word 原生 `<m:oMath>` 公式对象。\n\n"
            "$$\\mathcal{L}_{total} = \\sum_{i=1}^{N} (y_i - \\hat{y}_i)^2 + \\lambda \\|\\mathbf{W}\\|_2^2$$\n\n"
            "> 提示：在右侧顶部切换「Word 视图 / Markdown 源码 / LaTeX 公式」，右键可直接导出 `.docx` 或 `.pdf`。\n";
        write_file_utf8(path_to_utf8(root_p / utf8_to_path("算法仓库/核心代码/公式笔记.md")), formula_md);

        std::string vocab_md =
            "# English 生词本 (Vocabulary)\n\n"
            "由 OmniWorkbench English 工作台自动同步记录。\n\n"
            "## insight `/ˈɪnsaɪt/`\n"
            "- **释义**: n. 洞察力；顿悟；深刻见解\n"
            "- **笔记**: 词根拆解 in (向内) + sight (看见) → 看透本质。\n";
        write_file_utf8(path_to_utf8(root_p / utf8_to_path("English/生词沉淀/Vocabulary.md")), vocab_md);
    }
}

JsonValue VaultManager::scan_tree() const {
    JsonValue res = JsonValue::make_object();
    fs::path root_p = utf8_to_path(root_path_);
    std::string root_name = path_to_utf8(root_p.filename());
    if (root_name.empty()) root_name = root_path_;

    res["root_path"] = root_path_;
    res["root_name"] = root_name;

    JsonValue categories = JsonValue::make_array();
    JsonValue all_files = JsonValue::make_array();

    std::error_code ec;
    if (!fs::exists(root_p, ec)) {
        res["categories"] = std::move(categories);
        res["files"] = std::move(all_files);
        return res;
    }

    auto build_file_json = [&](const fs::path& fp) {
        std::string rel = path_to_utf8(fs::relative(fp, root_p, ec));
        for (char& c : rel) if (c == '\\') c = '/';
        std::string ext = lower_ext(fp);
        JsonValue f = JsonValue::make_object();
        f["name"] = path_to_utf8(fp.filename());
        f["stem"] = path_to_utf8(fp.stem());
        f["ext"] = ext;
        f["rel_path"] = rel;
        std::string title_p, snippet_p;
        extract_doc_preview(fp, ext, title_p, snippet_p);
        f["preview_title"] = title_p;
        f["preview"] = snippet_p;
        uintmax_t sz = fs::file_size(fp, ec);
        f["size"] = static_cast<int64_t>(ec ? 0 : sz);
        return f;
    };

    std::function<JsonValue(const fs::path&)> build_dir_tree = [&](const fs::path& dp) -> JsonValue {
        JsonValue node = JsonValue::make_object();
        std::string rel = (dp == root_p) ? "" : path_to_utf8(fs::relative(dp, root_p, ec));
        for (char& c : rel) if (c == '\\') c = '/';
        node["name"] = (dp == root_p) ? root_name : path_to_utf8(dp.filename());
        node["rel_path"] = rel;
        JsonValue dirs = JsonValue::make_array();
        JsonValue files = JsonValue::make_array();
        for (const auto& item : list_sorted_children(dp)) {
            if (item.is_dir) dirs.push_back(build_dir_tree(item.path));
            else files.push_back(build_file_json(item.path));
        }
        node["dirs"] = std::move(dirs);
        node["files"] = std::move(files);
        return node;
    };
    res["tree_root"] = build_dir_tree(root_p);

    // Walk all files for flat lookup / search
    for (fs::recursive_directory_iterator it(root_p, ec), end; it != end; it.increment(ec)) {
        if (ec) continue;
        std::string name = path_to_utf8(it->path().filename());
        if (is_hidden_name(name)) {
            if (it->is_directory(ec)) it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file(ec) && is_supported_doc_ext(lower_ext(it->path()))) {
            all_files.push_back(build_file_json(it->path()));
        }
    }

    // Build 4-layer hierarchy: Root -> Layer2 (Cat) -> Layer3 (Subdir) -> Layer4 (Files)
    auto top_items = list_sorted_children(root_p);
    JsonValue root_direct_files = JsonValue::make_array();

    for (const auto& item : top_items) {
        if (!item.is_dir) {
            root_direct_files.push_back(build_file_json(item.path));
            continue;
        }

        JsonValue cat = JsonValue::make_object();
        cat["name"] = item.name;
        std::string cat_rel = path_to_utf8(fs::relative(item.path, root_p, ec));
        for (char& c : cat_rel) if (c == '\\') c = '/';
        cat["rel_path"] = cat_rel;

        JsonValue subdirs = JsonValue::make_array();
        JsonValue direct_files = JsonValue::make_array();

        for (const auto& sub : list_sorted_children(item.path)) {
            if (sub.is_dir) {
                JsonValue sd = JsonValue::make_object();
                sd["name"] = sub.name;
                std::string sd_rel = path_to_utf8(fs::relative(sub.path, root_p, ec));
                for (char& c : sd_rel) if (c == '\\') c = '/';
                sd["rel_path"] = sd_rel;

                JsonValue sfiles = JsonValue::make_array();
                // Collect files inside subdir (including deeper nested ones flattened at layer 4)
                for (fs::recursive_directory_iterator rit(sub.path, ec), rend; rit != rend; rit.increment(ec)) {
                    if (ec) continue;
                    std::string fn = path_to_utf8(rit->path().filename());
                    if (is_hidden_name(fn)) {
                        if (rit->is_directory(ec)) rit.disable_recursion_pending();
                        continue;
                    }
                    if (rit->is_regular_file(ec) && is_supported_doc_ext(lower_ext(rit->path()))) {
                        sfiles.push_back(build_file_json(rit->path()));
                    }
                }
                sd["files"] = std::move(sfiles);
                subdirs.push_back(std::move(sd));
            } else {
                direct_files.push_back(build_file_json(sub.path));
            }
        }

        cat["subdirs"] = std::move(subdirs);
        cat["files"] = std::move(direct_files);
        categories.push_back(std::move(cat));
    }

    if (!root_direct_files.arr_val.empty()) {
        JsonValue misc = JsonValue::make_object();
        misc["name"] = "根目录文档";
        misc["rel_path"] = "";
        misc["subdirs"] = JsonValue::make_array();
        misc["files"] = std::move(root_direct_files);
        categories.push_back(std::move(misc));
    }

    res["categories"] = std::move(categories);
    res["files"] = std::move(all_files);
    return res;
}

std::string VaultManager::resolve_safe_path(const std::string& rel_or_abs) const {
    fs::path input = utf8_to_path(rel_or_abs);
    if (input.is_absolute()) {
        return path_to_utf8(input);
    }
    fs::path combined = utf8_to_path(root_path_) / input;
    return path_to_utf8(combined);
}

bool VaultManager::open_document(const std::string& rel_or_abs_path, VaultFileContent& out, std::string* err) const {
    std::string abs = resolve_safe_path(rel_or_abs_path);
    fs::path p = utf8_to_path(abs);
    std::error_code ec;
    if (!fs::exists(p, ec)) {
        if (err) *err = "File not found: " + abs;
        return false;
    }

    out.abs_path = abs;
    fs::path rel = fs::relative(p, utf8_to_path(root_path_), ec);
    out.rel_path = ec ? path_to_utf8(p.filename()) : path_to_utf8(rel);
    for (char& c : out.rel_path) if (c == '\\') c = '/';
    out.ext = lower_ext(p);
    out.is_docx = (out.ext == ".docx");

    if (out.is_docx) {
        if (!docx_to_markdown(abs, out.markdown, err)) {
            return false;
        }
    } else {
        if (!read_file_utf8(abs, out.markdown, err)) {
            return false;
        }
    }

    MdDocument doc = parse_markdown(out.markdown);
    out.html = render_markdown_to_html(doc, false, "OmniWorkbench Document", out.rel_path);
    return true;
}

bool VaultManager::save_document(const std::string& rel_or_abs_path,
                                 const std::string& markdown_content,
                                 std::string* err,
                                 const DocxExportContext* ctx) const {
    std::string abs = resolve_safe_path(rel_or_abs_path);
    fs::path p = utf8_to_path(abs);
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);

    if (lower_ext(p) == ".docx") {
        DocxExportContext local_ctx;
        if (ctx) local_ctx = *ctx;
        if (local_ctx.base_dir.empty()) {
            local_ctx.base_dir = path_to_utf8(p.has_parent_path() ? p.parent_path() : utf8_to_path(root_path_));
        }
        return markdown_to_docx(markdown_content, abs, err, &local_ctx);
    }
    return write_file_utf8(abs, markdown_content, err);
}

bool VaultManager::create_note(const std::string& rel_path, const std::string& initial_md, std::string* err) const {
    std::string normalized = rel_path;
    if (normalized.find('.') == std::string::npos) normalized += ".md";
    std::string abs = resolve_safe_path(normalized);
    fs::path p = utf8_to_path(abs);
    std::error_code ec;
    if (fs::exists(p, ec)) {
        if (err) *err = "File already exists: " + normalized;
        return false;
    }
    if (lower_ext(p) == ".docx") {
        return markdown_to_docx(initial_md, abs, err);
    }
    return write_file_utf8(abs, initial_md, err);
}

bool VaultManager::create_folder(const std::string& rel_path, std::string* err) const {
    std::string abs = resolve_safe_path(rel_path);
    std::error_code ec;
    if (fs::create_directories(utf8_to_path(abs), ec)) {
        return true;
    }
    if (fs::exists(utf8_to_path(abs), ec)) return true;
    if (err) *err = "Failed to create folder: " + ec.message();
    return false;
}

std::string VaultManager::resolve_wikilink(const std::string& name) const {
    std::error_code ec;
    fs::path root_p = utf8_to_path(root_path_);
    if (!fs::exists(root_p, ec)) return "";

    std::string target = name;
    for (char& c : target) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string fallback_match;
    for (fs::recursive_directory_iterator it(root_p, ec), end; it != end; it.increment(ec)) {
        if (ec || !it->is_regular_file(ec)) continue;
        std::string ext = lower_ext(it->path());
        if (!is_supported_doc_ext(ext)) continue;

        std::string stem = path_to_utf8(it->path().stem());
        std::string fname = path_to_utf8(it->path().filename());
        std::string stem_l = stem;
        std::string fname_l = fname;
        for (char& c : stem_l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        for (char& c : fname_l) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        std::string rel = path_to_utf8(fs::relative(it->path(), root_p, ec));
        for (char& c : rel) if (c == '\\') c = '/';

        if (stem_l == target || fname_l == target) {
            if (ext == ".md") return rel;
            fallback_match = rel;
        }
    }
    return fallback_match;
}

bool VaultManager::convert_document(const std::string& source_rel_or_abs,
                                    const std::string& markdown_override,
                                    const std::string& target_format,
                                    const std::string& out_path,
                                    std::string* err,
                                    const DocxExportContext* ctx) const {
    std::string md = markdown_override;
    std::string src_abs = resolve_safe_path(source_rel_or_abs);
    fs::path src_p = utf8_to_path(src_abs);

    if (md.empty() && !source_rel_or_abs.empty()) {
        VaultFileContent loaded;
        if (!open_document(source_rel_or_abs, loaded, err)) {
            return false;
        }
        md = loaded.markdown;
    }

    std::string out_abs = resolve_safe_path(out_path);
    if (target_format == "docx") {
        DocxExportContext local_ctx;
        if (ctx) local_ctx = *ctx;
        if (local_ctx.base_dir.empty()) {
            fs::path b = (!source_rel_or_abs.empty() && src_p.has_parent_path())
                             ? src_p.parent_path()
                             : utf8_to_path(root_path_);
            local_ctx.base_dir = path_to_utf8(b);
        }
        return markdown_to_docx(md, out_abs, err, &local_ctx);
    }
    if (target_format == "md") {
        if (lower_ext(src_p) == ".docx" && markdown_override.empty()) {
            std::string extracted;
            if (!docx_to_markdown(src_abs, extracted, err)) return false;
            return write_file_utf8(out_abs, extracted, err);
        }
        return write_file_utf8(out_abs, md, err);
    }
    if (target_format == "pdf") {
        std::string title = path_to_utf8(src_p.stem());
        if (title.empty()) title = "OmniWorkbench Document";
        return markdown_to_pdf(md, out_abs, title, err);
    }

    if (err) *err = "Unsupported conversion format: " + target_format;
    return false;
}

bool VaultManager::append_vocabulary_entry(const std::string& word,
                                           const std::string& ipa,
                                           const std::string& pos,
                                           const std::string& note,
                                           std::string* err) const {
    fs::path vocab_p = utf8_to_path(root_path_) / utf8_to_path("English/生词沉淀/Vocabulary.md");
    std::error_code ec;
    if (!fs::exists(vocab_p, ec)) {
        // Also check English/Vocabulary.md
        fs::path alt_p = utf8_to_path(root_path_) / utf8_to_path("English/Vocabulary.md");
        if (fs::exists(alt_p, ec)) vocab_p = alt_p;
    }

    std::string existing;
    read_file_utf8(path_to_utf8(vocab_p), existing);
    if (existing.empty()) {
        existing = "# English 生词本 (Vocabulary)\n\n由 OmniWorkbench English 工作台自动同步记录。\n";
    }

    std::ostringstream oss;
    oss << existing;
    if (!existing.empty() && existing.back() != '\n') oss << "\n";
    oss << "\n## " << word << " `" << ipa << "`\n";
    if (!pos.empty()) oss << "- **释义**: " << pos << "\n";
    if (!note.empty()) {
        std::istringstream niss(note);
        std::string nl;
        while (std::getline(niss, nl)) {
            if (!nl.empty()) oss << "- " << nl << "\n";
        }
    }

    return write_file_utf8(path_to_utf8(vocab_p), oss.str(), err);
}

} // namespace omni
