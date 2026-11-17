#include "markdown.hpp"
#include "json.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

namespace omni {

namespace {

std::string trim(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
    size_t b = s.size();
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
    return s.substr(a, b - a);
}

std::string to_lower_ascii(std::string s) {
    for (char& c : s) {
        if (static_cast<unsigned char>(c) < 128) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    return s;
}

std::string url_encode(const std::string& s) {
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char c : s) {
        if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~' || c == '/') {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('%');
            out.push_back(hex[(c >> 4) & 0xF]);
            out.push_back(hex[c & 0xF]);
        }
    }
    return out;
}

int leading_indent_spaces(const std::string& line) {
    int sp = 0;
    for (char c : line) {
        if (c == ' ') ++sp;
        else if (c == '\t') sp += 4;
        else break;
    }
    return sp;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::string cur;
    for (size_t i = 0; i < text.size(); ++i) {
        char c = text[i];
        if (c == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') ++i;
            lines.push_back(std::move(cur));
            cur.clear();
        } else if (c == '\n') {
            lines.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    lines.push_back(std::move(cur));
    return lines;
}

bool is_hr_line(const std::string& t) {
    if (t.size() < 3) return false;
    char ch = 0;
    int count = 0;
    for (char c : t) {
        if (c == ' ' || c == '\t') continue;
        if (c != '-' && c != '*' && c != '_') return false;
        if (ch == 0) ch = c;
        else if (c != ch) return false;
        ++count;
    }
    return count >= 3;
}

std::vector<std::string> split_table_cells(const std::string& line) {
    std::string t = trim(line);
    if (!t.empty() && t.front() == '|') t.erase(t.begin());
    if (!t.empty() && t.back() == '|') t.pop_back();

    std::vector<std::string> cells;
    std::string cur;
    for (size_t i = 0; i < t.size(); ++i) {
        if (t[i] == '\\' && i + 1 < t.size() && t[i + 1] == '|') {
            cur.push_back('|');
            ++i;
        } else if (t[i] == '|') {
            cells.push_back(trim(cur));
            cur.clear();
        } else {
            cur.push_back(t[i]);
        }
    }
    cells.push_back(trim(cur));
    return cells;
}

bool is_table_separator(const std::string& line, std::vector<std::string>& aligns) {
    if (line.find('|') == std::string::npos) return false;
    auto cells = split_table_cells(line);
    if (cells.empty()) return false;
    aligns.clear();
    for (const auto& c : cells) {
        if (c.empty()) return false;
        bool left = (c.front() == ':');
        bool right = (c.back() == ':');
        size_t start = left ? 1 : 0;
        size_t end = right ? c.size() - 1 : c.size();
        if (end <= start) return false;
        for (size_t i = start; i < end; ++i) {
            if (c[i] != '-') return false;
        }
        if (left && right) aligns.push_back("center");
        else if (right) aligns.push_back("right");
        else aligns.push_back("left");
    }
    return true;
}

bool parse_ordered_prefix(const std::string& t, int& num, std::string& rest) {
    size_t i = 0;
    while (i < t.size() && std::isdigit(static_cast<unsigned char>(t[i]))) ++i;
    if (i == 0 || i + 1 >= t.size()) return false;
    if ((t[i] == '.' || t[i] == ')') && t[i + 1] == ' ') {
        try {
            num = std::stoi(t.substr(0, i));
        } catch (...) {
            num = 1;
        }
        rest = trim(t.substr(i + 2));
        return true;
    }
    return false;
}

bool parse_task_prefix(const std::string& t, bool& checked, std::string& rest) {
    if (t.size() >= 6 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ' &&
        t[2] == '[' && (t[3] == ' ' || t[3] == 'x' || t[3] == 'X') && t[4] == ']' && t[5] == ' ') {
        checked = (t[3] == 'x' || t[3] == 'X');
        rest = trim(t.substr(6));
        return true;
    }
    return false;
}

// ============================================================================
// Rich Offline TeX -> HTML Renderer
// ============================================================================
class TexToHtml {
public:
    explicit TexToHtml(const std::string& s) : s_(s), pos_(0) {}

    std::string render() {
        return parse_sequence(0);
    }

private:
    const std::string& s_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    static std::string map_symbol(const std::string& cmd) {
        static const std::map<std::string, std::string> syms = {
            {"alpha", "&alpha;"}, {"beta", "&beta;"}, {"gamma", "&gamma;"}, {"delta", "&delta;"},
            {"epsilon", "&#1013;"}, {"varepsilon", "&epsilon;"}, {"zeta", "&zeta;"}, {"eta", "&eta;"},
            {"theta", "&theta;"}, {"iota", "&iota;"}, {"kappa", "&kappa;"}, {"lambda", "&lambda;"},
            {"mu", "&mu;"}, {"nu", "&nu;"}, {"xi", "&xi;"}, {"pi", "&pi;"},
            {"rho", "&rho;"}, {"sigma", "&sigma;"}, {"tau", "&tau;"}, {"upsilon", "&upsilon;"},
            {"phi", "&phi;"}, {"varphi", "&#981;"}, {"chi", "&chi;"}, {"psi", "&psi;"}, {"omega", "&omega;"},
            {"Gamma", "&Gamma;"}, {"Delta", "&Delta;"}, {"Theta", "&Theta;"}, {"Lambda", "&Lambda;"},
            {"Xi", "&Xi;"}, {"Pi", "&Pi;"}, {"Sigma", "&Sigma;"}, {"Phi", "&Phi;"}, {"Psi", "&Psi;"}, {"Omega", "&Omega;"},
            {"infty", "&infin;"}, {"partial", "&part;"}, {"nabla", "&nabla;"}, {"cdot", "&middot;"},
            {"times", "&times;"}, {"div", "&divide;"}, {"pm", "&plusmn;"}, {"mp", "&#8723;"},
            {"leq", "&le;"}, {"le", "&le;"}, {"geq", "&ge;"}, {"ge", "&ge;"},
            {"neq", "&ne;"}, {"ne", "&ne;"}, {"approx", "&approx;"}, {"equiv", "&equiv;"},
            {"to", "&rarr;"}, {"rightarrow", "&rarr;"}, {"leftarrow", "&larr;"}, {"Rightarrow", "&rArr;"},
            {"Leftrightarrow", "&hArr;"}, {"leftrightarrow", "&harr;"},
            {"sum", "&sum;"}, {"prod", "&prod;"}, {"int", "&int;"}, {"oint", "&#8750;"},
            {"forall", "&forall;"}, {"exists", "&exist;"}, {"in", "&isin;"}, {"notin", "&notin;"},
            {"subset", "&sub;"}, {"subseteq", "&sube;"}, {"cup", "&cup;"}, {"cap", "&cap;"},
            {"quad", "&nbsp;&nbsp;"}, {"qquad", "&nbsp;&nbsp;&nbsp;&nbsp;"}, {",", "&thinsp;"}, {";", "&ensp;"}, {"|", "&#8214;"},
            {"{", "{"}, {"}", "}"}
        };
        auto it = syms.find(cmd);
        return it != syms.end() ? it->second : "";
    }

    static std::string map_mathbb(const std::string& inner_raw) {
        std::string out;
        for (char c : inner_raw) {
            switch (c) {
                case 'R': out += "&#8477;"; break;
                case 'N': out += "&#8469;"; break;
                case 'Z': out += "&#8484;"; break;
                case 'Q': out += "&#8474;"; break;
                case 'C': out += "&#8450;"; break;
                case 'E': out += "&#120124;"; break;
                case 'P': out += "&#8473;"; break;
                default: out.push_back(c); break;
            }
        }
        return "<span class=\"omni-mbb\">" + out + "</span>";
    }

    std::string read_braced_raw() {
        skip_ws();
        if (pos_ >= s_.size()) return "";
        if (s_[pos_] != '{') {
            return std::string(1, s_[pos_++]);
        }
        ++pos_;
        int depth = 1;
        std::string out;
        while (pos_ < s_.size() && depth > 0) {
            char c = s_[pos_++];
            if (c == '{') ++depth;
            else if (c == '}') {
                --depth;
                if (depth == 0) break;
            }
            out.push_back(c);
        }
        return out;
    }

    std::string parse_group() {
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '{') {
            ++pos_;
            return parse_sequence('}');
        }
        return parse_atom();
    }

    std::string render_env(const std::string& env_name, const std::string& body) {
        // Split rows by "\\"
        std::vector<std::vector<std::string>> rows;
        std::vector<std::string> cur_row;
        std::string cell;
        for (size_t i = 0; i < body.size(); ++i) {
            if (body[i] == '\\' && i + 1 < body.size() && body[i + 1] == '\\') {
                cur_row.push_back(trim(cell));
                cell.clear();
                rows.push_back(std::move(cur_row));
                cur_row.clear();
                ++i;
            } else if (body[i] == '&') {
                cur_row.push_back(trim(cell));
                cell.clear();
            } else {
                cell.push_back(body[i]);
            }
        }
        if (!trim(cell).empty() || !cur_row.empty()) {
            cur_row.push_back(trim(cell));
            rows.push_back(std::move(cur_row));
        }

        std::string ldelim, rdelim;
        if (env_name == "pmatrix") { ldelim = "("; rdelim = ")"; }
        else if (env_name == "bmatrix") { ldelim = "["; rdelim = "]"; }
        else if (env_name == "vmatrix") { ldelim = "|"; rdelim = "|"; }
        else if (env_name == "Bmatrix") { ldelim = "{"; rdelim = "}"; }
        else if (env_name == "cases") { ldelim = "{"; rdelim = ""; }

        std::ostringstream oss;
        oss << "<span class=\"omni-mmatrix-wrap\">";
        if (!ldelim.empty()) oss << "<span class=\"omni-mdelim\">" << ldelim << "</span>";
        oss << "<span class=\"omni-mmatrix\">";
        for (const auto& r : rows) {
            oss << "<span class=\"omni-mrow\">";
            for (const auto& c : r) {
                TexToHtml sub(c);
                oss << "<span class=\"omni-mcell\">" << sub.render() << "</span>";
            }
            oss << "</span>";
        }
        oss << "</span>";
        if (!rdelim.empty()) oss << "<span class=\"omni-mdelim\">" << rdelim << "</span>";
        oss << "</span>";
        return oss.str();
    }

    std::string parse_sequence(char stop_char) {
        std::string out;
        while (pos_ < s_.size()) {
            if (stop_char != 0 && s_[pos_] == stop_char) {
                ++pos_;
                break;
            }
            if (std::isspace(static_cast<unsigned char>(s_[pos_]))) {
                ++pos_;
                continue;
            }
            out += parse_atom_with_scripts();
        }
        return out;
    }

    std::string parse_atom_with_scripts() {
        // Check if this is a large operator so we can format sub/sup cleanly
        size_t save_pos = pos_;
        skip_ws();
        bool is_nary = false;
        if (pos_ < s_.size() && s_[pos_] == '\\') {
            size_t j = pos_ + 1;
            while (j < s_.size() && std::isalpha(static_cast<unsigned char>(s_[j]))) ++j;
            std::string cmd = s_.substr(pos_ + 1, j - pos_ - 1);
            if (cmd == "sum" || cmd == "prod" || cmd == "int" || cmd == "oint") {
                is_nary = true;
            }
        }
        pos_ = save_pos;

        std::string base = parse_atom();
        bool has_sub = false, has_sup = false;
        std::string sub, sup;
        for (int pass = 0; pass < 2; ++pass) {
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == '_' && !has_sub) {
                ++pos_;
                sub = parse_group();
                has_sub = true;
            } else if (pos_ < s_.size() && s_[pos_] == '^' && !has_sup) {
                ++pos_;
                sup = parse_group();
                has_sup = true;
            } else {
                break;
            }
        }

        if (is_nary && (has_sub || has_sup)) {
            std::string out = "<span class=\"omni-mnary\"><span class=\"omni-mnary-op\">" + base + "</span>";
            out += "<span class=\"omni-mnary-lims\"><span class=\"omni-msup\">" + (has_sup ? sup : "&nbsp;") + "</span>";
            out += "<span class=\"omni-msub\">" + (has_sub ? sub : "&nbsp;") + "</span></span></span>";
            return out;
        }
        if (has_sub && has_sup) {
            return base + "<span class=\"omni-msupsub\"><span class=\"omni-msup\">" + sup + "</span><span class=\"omni-msub\">" + sub + "</span></span>";
        }
        if (has_sup) return base + "<sup class=\"omni-mscript\">" + sup + "</sup>";
        if (has_sub) return base + "<sub class=\"omni-mscript\">" + sub + "</sub>";
        return base;
    }

    std::string parse_atom() {
        skip_ws();
        if (pos_ >= s_.size()) return "";
        char c = s_[pos_];

        if (c == '{') {
            ++pos_;
            return parse_sequence('}');
        }
        if (c == '\\') {
            ++pos_;
            if (pos_ >= s_.size()) return "";
            std::string cmd;
            if (!std::isalpha(static_cast<unsigned char>(s_[pos_]))) {
                cmd.push_back(s_[pos_++]);
            } else {
                while (pos_ < s_.size() && std::isalpha(static_cast<unsigned char>(s_[pos_]))) {
                    cmd.push_back(s_[pos_++]);
                }
            }

            if (cmd == "frac" || cmd == "dfrac" || cmd == "tfrac") {
                std::string num = parse_group();
                std::string den = parse_group();
                return "<span class=\"omni-mfrac\"><span class=\"omni-mnum\">" + num +
                       "</span><span class=\"omni-mden\">" + den + "</span></span>";
            }
            if (cmd == "sqrt") {
                skip_ws();
                std::string deg;
                if (pos_ < s_.size() && s_[pos_] == '[') {
                    ++pos_;
                    deg = parse_sequence(']');
                }
                std::string rad = parse_group();
                std::string out = "<span class=\"omni-msqrt\">";
                if (!deg.empty()) out += "<sup class=\"omni-mdeg\">" + deg + "</sup>";
                out += "<span class=\"omni-mrad-sym\">&radic;</span><span class=\"omni-mrad-body\">" + rad + "</span></span>";
                return out;
            }
            if (cmd == "begin") {
                std::string env = trim(read_braced_raw());
                std::string end_tag = "\\end{" + env + "}";
                size_t end_pos = s_.find(end_tag, pos_);
                std::string body;
                if (end_pos == std::string::npos) {
                    body = s_.substr(pos_);
                    pos_ = s_.size();
                } else {
                    body = s_.substr(pos_, end_pos - pos_);
                    pos_ = end_pos + end_tag.size();
                }
                return render_env(env, body);
            }
            if (cmd == "mathbb") {
                return map_mathbb(read_braced_raw());
            }
            if (cmd == "mathbf") return "<strong>" + parse_group() + "</strong>";
            if (cmd == "mathrm" || cmd == "text" || cmd == "operatorname") {
                if (cmd == "text") return "<span class=\"omni-mtext\">" + html_escape(read_braced_raw()) + "</span>";
                return "<span class=\"omni-mtext\">" + parse_group() + "</span>";
            }
            if (cmd == "mathcal") return "<span style=\"font-style:italic;\">" + parse_group() + "</span>";
            if (cmd == "hat") return "<span class=\"omni-maccent\">" + parse_group() + "<span class=\"omni-maccent-mk\">^</span></span>";
            if (cmd == "bar" || cmd == "overline") return "<span class=\"omni-mover\">" + parse_group() + "</span>";
            if (cmd == "vec") return "<span class=\"omni-maccent\">" + parse_group() + "<span class=\"omni-maccent-mk\">&rarr;</span></span>";
            if (cmd == "tilde") return "<span class=\"omni-maccent\">" + parse_group() + "<span class=\"omni-maccent-mk\">~</span></span>";
            if (cmd == "dot") return "<span class=\"omni-maccent\">" + parse_group() + "<span class=\"omni-maccent-mk\">&middot;</span></span>";
            if (cmd == "left" || cmd == "right") {
                skip_ws();
                if (pos_ < s_.size()) {
                    char delim = s_[pos_++];
                    if (delim == '\\') {
                        if (pos_ < s_.size() && (s_[pos_] == '{' || s_[pos_] == '}' || s_[pos_] == '|')) {
                            char d2 = s_[pos_++];
                            return d2 == '|' ? "&#8214;" : std::string(1, d2);
                        }
                    }
                    if (delim == '.') return "";
                    return html_escape(std::string(1, delim));
                }
                return "";
            }
            if (cmd == "sin" || cmd == "cos" || cmd == "tan" || cmd == "log" || cmd == "ln" ||
                cmd == "exp" || cmd == "lim" || cmd == "max" || cmd == "min" || cmd == "sup" ||
                cmd == "inf" || cmd == "arg" || cmd == "det" || cmd == "dim") {
                return "<span class=\"omni-mtext\">" + cmd + "</span>";
            }
            std::string sym = map_symbol(cmd);
            if (!sym.empty()) return sym;
            return html_escape(cmd);
        }

        // Plain char / UTF-8 sequence
        unsigned char uc = static_cast<unsigned char>(c);
        size_t len = 1;
        if ((uc & 0xE0) == 0xC0) len = 2;
        else if ((uc & 0xF0) == 0xE0) len = 3;
        else if ((uc & 0xF8) == 0xF0) len = 4;
        std::string ch = s_.substr(pos_, std::min(len, s_.size() - pos_));
        pos_ += ch.size();
        if (ch == "=" || ch == "+" || ch == "-" || ch == "<" || ch == ">") {
            return " " + html_escape(ch) + " ";
        }
        if (std::isalpha(uc)) {
            return "<var class=\"omni-mvar\">" + html_escape(ch) + "</var>";
        }
        return html_escape(ch);
    }
};

// ============================================================================
// Composable Recursive Inline Markdown Parser
// ============================================================================
bool same_style(const InlineSpan& a, const InlineSpan& b) {
    return a.bold == b.bold && a.italic == b.italic && a.strike == b.strike &&
           a.underline == b.underline && a.sup == b.sup && a.sub == b.sub &&
           a.code == b.code && a.color == b.color && a.highlight == b.highlight &&
           a.font_family == b.font_family && std::abs(a.font_size_pt - b.font_size_pt) < 0.01;
}

void push_styled_text(std::vector<InlineSpan>& out, const std::string& s, const InlineSpan& tmpl) {
    if (s.empty()) return;
    InlineSpan sp = tmpl;
    sp.kind = InlineSpan::Kind::Text;
    sp.text = s;
    sp.url.clear();
    if (!out.empty() && out.back().kind == InlineSpan::Kind::Text && same_style(out.back(), sp)) {
        out.back().text += s;
    } else {
        out.push_back(std::move(sp));
    }
}

std::string extract_attr_value(const std::string& attrs, const std::string& key) {
    std::string low = to_lower_ascii(attrs);
    std::string k = to_lower_ascii(key);
    size_t pos = 0;
    while ((pos = low.find(k, pos)) != std::string::npos) {
        if (pos > 0 && std::isalnum(static_cast<unsigned char>(low[pos - 1]))) {
            pos += k.size();
            continue;
        }
        size_t eq = pos + k.size();
        while (eq < attrs.size() && std::isspace(static_cast<unsigned char>(attrs[eq]))) ++eq;
        if (eq >= attrs.size() || attrs[eq] != '=') {
            pos += k.size();
            continue;
        }
        ++eq;
        while (eq < attrs.size() && std::isspace(static_cast<unsigned char>(attrs[eq]))) ++eq;
        if (eq >= attrs.size()) return "";
        char quote = attrs[eq];
        if (quote == '"' || quote == '\'') {
            size_t end = attrs.find(quote, eq + 1);
            if (end != std::string::npos) return attrs.substr(eq + 1, end - eq - 1);
        } else {
            size_t end = eq;
            while (end < attrs.size() && !std::isspace(static_cast<unsigned char>(attrs[end])) && attrs[end] != '>') ++end;
            return attrs.substr(eq, end - eq);
        }
        break;
    }
    return "";
}

void apply_css_style(const std::string& style_str, InlineSpan& st) {
    std::istringstream iss(style_str);
    std::string decl;
    while (std::getline(iss, decl, ';')) {
        size_t colon = decl.find(':');
        if (colon == std::string::npos) continue;
        std::string prop = to_lower_ascii(trim(decl.substr(0, colon)));
        std::string val = trim(decl.substr(colon + 1));
        if (val.empty()) continue;
        if (prop == "color") {
            st.color = val;
        } else if (prop == "background-color" || prop == "background") {
            st.highlight = val;
        } else if (prop == "font-family") {
            // Strip surrounding quotes and take first family name
            size_t comma = val.find(',');
            std::string first = (comma == std::string::npos) ? val : val.substr(0, comma);
            first = trim(first);
            if (first.size() >= 2 && ((first.front() == '"' && first.back() == '"') ||
                                      (first.front() == '\'' && first.back() == '\''))) {
                first = first.substr(1, first.size() - 2);
            }
            if (!first.empty()) st.font_family = first;
        } else if (prop == "font-size") {
            try {
                std::string lv = to_lower_ascii(val);
                double num = std::stod(lv);
                if (lv.find("px") != std::string::npos) st.font_size_pt = num * 0.75;
                else st.font_size_pt = num;
            } catch (...) {}
        } else if (prop == "text-decoration" && to_lower_ascii(val).find("underline") != std::string::npos) {
            st.underline = true;
        } else if (prop == "text-decoration" && to_lower_ascii(val).find("line-through") != std::string::npos) {
            st.strike = true;
        } else if (prop == "font-weight" && (to_lower_ascii(val) == "bold" || val == "700" || val == "600")) {
            st.bold = true;
        } else if (prop == "font-style" && to_lower_ascii(val) == "italic") {
            st.italic = true;
        }
    }
}

void parse_inline_rec(const std::string& text, const InlineSpan& cur_style, std::vector<InlineSpan>& out) {
    size_t i = 0;
    const size_t n = text.size();

    while (i < n) {
        // Escaped char
        if (text[i] == '\\' && i + 1 < n) {
            push_styled_text(out, std::string(1, text[i + 1]), cur_style);
            i += 2;
            continue;
        }

        // Line break HTML <br> / <br/> / <br />
        if (text[i] == '<') {
            std::string tail_low = to_lower_ascii(text.substr(i, std::min<size_t>(10, n - i)));
            if (tail_low.rfind("<br>", 0) == 0 || tail_low.rfind("<br/>", 0) == 0 || tail_low.rfind("<br />", 0) == 0) {
                size_t gt = text.find('>', i);
                InlineSpan lb = cur_style;
                lb.kind = InlineSpan::Kind::LineBreak;
                out.push_back(std::move(lb));
                i = gt + 1;
                continue;
            }
        }

        // Inline code ``...`` or `...`
        if (text[i] == '`') {
            size_t ticks = 1;
            if (i + 1 < n && text[i + 1] == '`') ticks = 2;
            std::string delim(ticks, '`');
            size_t end = text.find(delim, i + ticks);
            if (end != std::string::npos) {
                InlineSpan code_sp = cur_style;
                code_sp.code = true;
                push_styled_text(out, text.substr(i + ticks, end - (i + ticks)), code_sp);
                i = end + ticks;
                continue;
            }
        }

        // Inline math $...$ (not $$)
        if (text[i] == '$' && (i + 1 >= n || text[i + 1] != '$')) {
            size_t end = i + 1;
            while (end < n) {
                if (text[end] == '\\' && end + 1 < n) { end += 2; continue; }
                if (text[end] == '$') break;
                ++end;
            }
            if (end < n && text[end] == '$' && end > i + 1) {
                InlineSpan m = cur_style;
                m.kind = InlineSpan::Kind::InlineMath;
                m.text = text.substr(i + 1, end - i - 1);
                out.push_back(std::move(m));
                i = end + 1;
                continue;
            }
        }

        // Image ![alt](src)
        if (text[i] == '!' && i + 1 < n && text[i + 1] == '[') {
            size_t rb = text.find("](", i + 2);
            if (rb != std::string::npos) {
                size_t rp = text.find(')', rb + 2);
                if (rp != std::string::npos) {
                    InlineSpan img = cur_style;
                    img.kind = InlineSpan::Kind::Image;
                    img.text = text.substr(i + 2, rb - (i + 2));
                    img.url = trim(text.substr(rb + 2, rp - (rb + 2)));
                    out.push_back(std::move(img));
                    i = rp + 1;
                    continue;
                }
            }
        }

        // Wikilink [[target]] or [[target|label]]
        if (i + 1 < n && text[i] == '[' && text[i + 1] == '[') {
            size_t end = text.find("]]", i + 2);
            if (end != std::string::npos) {
                std::string inner = text.substr(i + 2, end - (i + 2));
                size_t bar = inner.find('|');
                std::string target = (bar == std::string::npos) ? inner : inner.substr(0, bar);
                std::string label = (bar == std::string::npos) ? inner : inner.substr(bar + 1);
                InlineSpan wk = cur_style;
                wk.kind = InlineSpan::Kind::Wikilink;
                wk.text = label;
                wk.url = target;
                out.push_back(std::move(wk));
                i = end + 2;
                continue;
            }
        }

        // Markdown link [label](url)
        if (text[i] == '[') {
            size_t rb = text.find("](", i + 1);
            if (rb != std::string::npos) {
                size_t rp = text.find(')', rb + 2);
                if (rp != std::string::npos) {
                    InlineSpan lnk = cur_style;
                    lnk.kind = InlineSpan::Kind::Link;
                    lnk.text = text.substr(i + 1, rb - (i + 1));
                    lnk.url = trim(text.substr(rb + 2, rp - (rb + 2)));
                    out.push_back(std::move(lnk));
                    i = rp + 1;
                    continue;
                }
            }
        }

        // Inline HTML tags: u, sup, sub, del, s, b, strong, i, em, mark, span
        if (text[i] == '<') {
            size_t gt = text.find('>', i + 1);
            if (gt != std::string::npos && gt > i + 1 && text[i + 1] != '/') {
                std::string tag_raw = text.substr(i + 1, gt - (i + 1));
                size_t sp_pos = tag_raw.find_first_of(" \t\r\n");
                std::string tag_name = to_lower_ascii(sp_pos == std::string::npos ? tag_raw : tag_raw.substr(0, sp_pos));
                std::string attrs = (sp_pos == std::string::npos) ? "" : tag_raw.substr(sp_pos + 1);

                bool supported_tag = (tag_name == "u" || tag_name == "sup" || tag_name == "sub" ||
                                      tag_name == "del" || tag_name == "s" || tag_name == "b" ||
                                      tag_name == "strong" || tag_name == "i" || tag_name == "em" ||
                                      tag_name == "mark" || tag_name == "span");
                if (supported_tag) {
                    // Find matching closing tag (handle nested same tag)
                    std::string open_pfx = "<" + tag_name;
                    std::string close_tag = "</" + tag_name + ">";
                    std::string low_text = to_lower_ascii(text);
                    int depth = 1;
                    size_t scan = gt + 1;
                    size_t close_pos = std::string::npos;
                    while (scan < n) {
                        size_t next_open = low_text.find(open_pfx, scan);
                        size_t next_close = low_text.find(close_tag, scan);
                        if (next_close == std::string::npos) break;
                        if (next_open != std::string::npos && next_open < next_close &&
                            next_open + open_pfx.size() < n &&
                            (low_text[next_open + open_pfx.size()] == '>' ||
                             std::isspace(static_cast<unsigned char>(low_text[next_open + open_pfx.size()])))) {
                            ++depth;
                            scan = next_open + open_pfx.size();
                        } else {
                            --depth;
                            if (depth == 0) {
                                close_pos = next_close;
                                break;
                            }
                            scan = next_close + close_tag.size();
                        }
                    }
                    if (close_pos != std::string::npos) {
                        InlineSpan next_st = cur_style;
                        if (tag_name == "u") next_st.underline = true;
                        else if (tag_name == "sup") next_st.sup = true;
                        else if (tag_name == "sub") next_st.sub = true;
                        else if (tag_name == "del" || tag_name == "s") next_st.strike = true;
                        else if (tag_name == "b" || tag_name == "strong") next_st.bold = true;
                        else if (tag_name == "i" || tag_name == "em") next_st.italic = true;
                        else if (tag_name == "mark") next_st.highlight = "#FEF08A";
                        std::string css = extract_attr_value(attrs, "style");
                        if (!css.empty()) apply_css_style(css, next_st);
                        parse_inline_rec(text.substr(gt + 1, close_pos - (gt + 1)), next_st, out);
                        i = close_pos + close_tag.size();
                        continue;
                    }
                }
            }
        }

        // Bold+Italic ***...*** or ___...___
        if (i + 2 < n && ((text[i] == '*' && text[i + 1] == '*' && text[i + 2] == '*') ||
                          (text[i] == '_' && text[i + 1] == '_' && text[i + 2] == '_'))) {
            std::string delim = text.substr(i, 3);
            size_t end = text.find(delim, i + 3);
            if (end != std::string::npos && end > i + 3) {
                InlineSpan next_st = cur_style;
                next_st.bold = true;
                next_st.italic = true;
                parse_inline_rec(text.substr(i + 3, end - (i + 3)), next_st, out);
                i = end + 3;
                continue;
            }
        }

        // Bold **...** or __...__
        if (i + 1 < n && ((text[i] == '*' && text[i + 1] == '*') || (text[i] == '_' && text[i + 1] == '_'))) {
            std::string delim = text.substr(i, 2);
            size_t end = text.find(delim, i + 2);
            if (end != std::string::npos && end > i + 2) {
                InlineSpan next_st = cur_style;
                next_st.bold = true;
                parse_inline_rec(text.substr(i + 2, end - (i + 2)), next_st, out);
                i = end + 2;
                continue;
            }
        }

        // Strikethrough ~~...~~
        if (i + 1 < n && text[i] == '~' && text[i + 1] == '~') {
            size_t end = text.find("~~", i + 2);
            if (end != std::string::npos && end > i + 2) {
                InlineSpan next_st = cur_style;
                next_st.strike = true;
                parse_inline_rec(text.substr(i + 2, end - (i + 2)), next_st, out);
                i = end + 2;
                continue;
            }
        }

        // Italic *...* or _..._
        if (text[i] == '*' || text[i] == '_') {
            char delim = text[i];
            size_t end = text.find(delim, i + 1);
            if (end != std::string::npos && end > i + 1) {
                InlineSpan next_st = cur_style;
                next_st.italic = true;
                parse_inline_rec(text.substr(i + 1, end - (i + 1)), next_st, out);
                i = end + 1;
                continue;
            }
        }

        push_styled_text(out, std::string(1, text[i]), cur_style);
        ++i;
    }
}

std::string resolve_preview_img_src(const std::string& src, bool standalone_a4, const std::string& asset_doc_path) {
    if (src.rfind("http://", 0) == 0 || src.rfind("https://", 0) == 0 ||
        src.rfind("data:", 0) == 0 || src.rfind("/api/", 0) == 0) {
        return src;
    }
    if (!standalone_a4 && !asset_doc_path.empty()) {
        return "/api/vault/asset?doc=" + url_encode(asset_doc_path) + "&path=" + url_encode(src);
    }
    return src;
}

std::string render_inlines_html(const std::vector<InlineSpan>& spans,
                                bool standalone_a4,
                                const std::string& asset_doc_path) {
    std::string out;
    for (const auto& sp : spans) {
        if (sp.kind == InlineSpan::Kind::LineBreak) {
            out += "<br/>";
            continue;
        }
        if (sp.kind == InlineSpan::Kind::Wikilink) {
            out += "<button type=\"button\" class=\"omni-wikilink\" contenteditable=\"false\" data-target=\"" +
                   html_escape(sp.url) + "\" onclick=\"openVaultByName('" +
                   html_escape(sp.url) + "')\">[[" + html_escape(sp.text) + "]]</button>";
            continue;
        }
        if (sp.kind == InlineSpan::Kind::InlineMath) {
            out += "<span class=\"omni-inline-math\" contenteditable=\"false\" data-tex=\"" + html_escape(sp.text) +
                   "\" title=\"双击编辑数学公式\">" + render_tex_to_html(sp.text, false) + "</span>";
            continue;
        }
        if (sp.kind == InlineSpan::Kind::Image) {
            std::string resolved = resolve_preview_img_src(sp.url, standalone_a4, asset_doc_path);
            out += "<span class=\"omni-img-wrap\" contenteditable=\"false\"><img class=\"omni-img\" src=\"" +
                   html_escape(resolved) + "\" data-raw-src=\"" + html_escape(sp.url) + "\" alt=\"" +
                   html_escape(sp.text) + "\"/></span>";
            continue;
        }

        std::string inner;
        if (sp.kind == InlineSpan::Kind::Link) {
            inner = "<a href=\"" + html_escape(sp.url) + "\" target=\"_blank\" class=\"omni-link\">" +
                    html_escape(sp.text) + "</a>";
        } else {
            inner = html_escape(sp.text);
        }

        if (sp.code) inner = "<code class=\"omni-inline-code\">" + inner + "</code>";
        if (sp.bold) inner = "<strong>" + inner + "</strong>";
        if (sp.italic) inner = "<em>" + inner + "</em>";
        if (sp.underline) inner = "<u>" + inner + "</u>";
        if (sp.strike) inner = "<del>" + inner + "</del>";
        if (sp.sup) inner = "<sup>" + inner + "</sup>";
        if (sp.sub) inner = "<sub>" + inner + "</sub>";
        if (!sp.highlight.empty()) {
            inner = "<mark class=\"omni-mark\" style=\"background-color:" + html_escape(sp.highlight) + "\">" + inner + "</mark>";
        }

        std::string style;
        if (!sp.color.empty()) style += "color:" + html_escape(sp.color) + ";";
        if (!sp.font_family.empty()) style += "font-family:'" + html_escape(sp.font_family) + "',sans-serif;";
        if (sp.font_size_pt > 0.0) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%.1fpt;", sp.font_size_pt);
            style += std::string("font-size:") + buf;
        }
        if (!style.empty()) {
            inner = "<span class=\"omni-styled-span\" style=\"" + style + "\">" + inner + "</span>";
        }
        out += inner;
    }
    return out;
}

std::string build_block_style_attrs(const BlockProps& p) {
    if (p.empty()) return "";
    std::ostringstream st;
    std::ostringstream da;
    if (!p.align.empty()) {
        st << "text-align:" << html_escape(p.align) << ";";
        da << " data-wp-align=\"" << html_escape(p.align) << "\"";
    }
    if (!p.line_rule.empty() && (p.line_rule == "atLeast" || p.line_rule == "exact") && p.line_pt > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", p.line_pt);
        st << "line-height:" << buf << "pt;";
        da << " data-wp-line-rule=\"" << html_escape(p.line_rule) << "\" data-wp-line-pt=\"" << buf << "\"";
    } else if (p.line_spacing > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.2f", p.line_spacing);
        st << "line-height:" << buf << ";";
        da << " data-wp-line=\"" << buf << "\"";
    }
    if (p.space_before_pt >= 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", p.space_before_pt);
        st << "margin-top:" << buf << "pt;";
        da << " data-wp-before=\"" << buf << "\"";
    }
    if (p.space_after_pt >= 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", p.space_after_pt);
        st << "margin-bottom:" << buf << "pt;";
        da << " data-wp-after=\"" << buf << "\"";
    }
    if (p.first_line_indent_chars > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", p.first_line_indent_chars);
        st << "text-indent:" << (p.first_line_indent_chars) << "em;";
        da << " data-wp-indent=\"" << buf << "\"";
    }
    if (!p.font_family.empty()) {
        st << "font-family:'" << html_escape(p.font_family) << "',serif;";
        da << " data-wp-font=\"" << html_escape(p.font_family) << "\"";
    }
    if (p.font_size_pt > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f", p.font_size_pt);
        st << "font-size:" << buf << "pt;";
        da << " data-wp-size=\"" << buf << "\"";
    }
    if (p.has_border) {
        st << "border:1px solid #CBD5E1;padding:6pt 9pt;border-radius:4px;";
        da << " data-wp-border=\"1\"";
    }
    if (!p.shading.empty()) {
        st << "background-color:" << html_escape(p.shading) << ";padding:4pt 8pt;border-radius:4px;";
        da << " data-wp-bg=\"" << html_escape(p.shading) << "\"";
    }
    std::string s_str = st.str();
    if (s_str.empty()) return "";
    return da.str() + " style=\"" + s_str + "\"";
}

} // namespace

std::string html_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&#39;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string render_tex_to_html(const std::string& tex, bool display_mode) {
    (void)display_mode;
    TexToHtml conv(tex);
    return conv.render();
}

std::string render_figma_json_to_svg(const std::string& json_str) {
    JsonValue doc;
    if (!JsonValue::parse(json_str, doc) || doc.type != JsonValue::Type::Object) {
        return "<div class=\"mono\" style=\"font-size:11px;color:#64748B;\">[Figma 算法可视化图配置解析失败]</div>";
    }

    int width = static_cast<int>(doc.get("width").as_int(680));
    int height = static_cast<int>(doc.get("height").as_int(360));
    if (width < 260) width = 680;
    if (height < 180) height = 360;
    std::string title = doc.get("title").as_string("算法流程可视化框");

    const auto& nodes = doc.get("nodes").arr_val;
    const auto& edges = doc.get("edges").arr_val;

    std::map<std::string, const JsonValue*> node_map;
    for (const auto& n : nodes) {
        std::string id = n.get("id").as_string();
        if (!id.empty()) node_map[id] = &n;
    }

    std::ostringstream svg;
    svg << "<svg viewBox=\"0 0 " << width << " " << height
        << "\" xmlns=\"http://www.w3.org/2000/svg\" style=\"width:100%;height:auto;max-height:420px;display:block;\">";
    svg << "<defs>"
        << "<marker id=\"omni-arrow\" viewBox=\"0 0 10 10\" refX=\"9\" refY=\"5\" markerWidth=\"7\" markerHeight=\"7\" orient=\"auto-start-reverse\">"
        << "<path d=\"M 0 1 L 10 5 L 0 9 z\" fill=\"#475569\"/></marker>"
        << "<pattern id=\"omni-grid\" width=\"20\" height=\"20\" patternUnits=\"userSpaceOnUse\">"
        << "<circle cx=\"2\" cy=\"2\" r=\"0.9\" fill=\"#CBD5E1\"/></pattern>"
        << "</defs>";
    svg << "<rect width=\"" << width << "\" height=\"" << height << "\" rx=\"10\" fill=\"#F8FAFC\" stroke=\"#E2E8F0\"/>";
    svg << "<rect width=\"" << width << "\" height=\"" << height << "\" rx=\"10\" fill=\"url(#omni-grid)\" opacity=\"0.65\"/>";
    svg << "<text x=\"16\" y=\"24\" font-size=\"12\" font-weight=\"700\" font-family=\"'黑体','Microsoft YaHei',sans-serif\" fill=\"#334155\">"
        << html_escape(title) << "</text>";

    // Draw frame containers first
    for (const auto& n : nodes) {
        if (n.get("shape").as_string() != "frame") continue;
        double x = n.get("x").as_double(40), y = n.get("y").as_double(40);
        double w = n.get("w").as_double(180), h = n.get("h").as_double(110);
        std::string stroke = n.get("stroke").as_string("#94A3B8");
        std::string label = n.get("label").as_string();
        svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
            << "\" rx=\"10\" fill=\"rgba(241,245,249,0.55)\" stroke=\"" << html_escape(stroke)
            << "\" stroke-width=\"1.5\" stroke-dasharray=\"5 4\"/>";
        if (!label.empty()) {
            svg << "<text x=\"" << (x + 10) << "\" y=\"" << (y + 18)
                << "\" font-size=\"11\" font-weight=\"600\" fill=\"#475569\">" << html_escape(label) << "</text>";
        }
    }

    // Draw edges
    for (const auto& e : edges) {
        auto it_from = node_map.find(e.get("from").as_string());
        auto it_to = node_map.find(e.get("to").as_string());
        if (it_from == node_map.end() || it_to == node_map.end()) continue;
        const JsonValue& fn = *it_from->second;
        const JsonValue& tn = *it_to->second;
        double x1 = fn.get("x").as_double(0) + fn.get("w").as_double(120) * 0.5;
        double y1 = fn.get("y").as_double(0) + fn.get("h").as_double(48) * 0.5;
        double x2 = tn.get("x").as_double(0) + tn.get("w").as_double(120) * 0.5;
        double y2 = tn.get("y").as_double(0) + tn.get("h").as_double(48) * 0.5;

        // Clip start/end points roughly to bounding boxes along direction
        double dx = x2 - x1, dy = y2 - y1;
        double sx = x1, sy = y1, ex = x2, ey = y2;
        double fw = fn.get("w").as_double(120) * 0.5, fh = fn.get("h").as_double(48) * 0.5;
        double tw = tn.get("w").as_double(120) * 0.5, th = tn.get("h").as_double(48) * 0.5;
        if (std::abs(dx) > 1e-3 || std::abs(dy) > 1e-3) {
            if (std::abs(dx) * fh > std::abs(dy) * fw) {
                sx = x1 + (dx > 0 ? fw : -fw);
                sy = y1 + dy * (fw / std::abs(dx));
            } else {
                sy = y1 + (dy > 0 ? fh : -fh);
                sx = x1 + dx * (fh / std::abs(dy));
            }
            if (std::abs(dx) * th > std::abs(dy) * tw) {
                ex = x2 - (dx > 0 ? tw : -tw);
                ey = y2 - dy * (tw / std::abs(dx));
            } else {
                ey = y2 - (dy > 0 ? th : -th);
                ex = x2 - dx * (th / std::abs(dy));
            }
        }
        bool dashed = e.get("dashed").as_bool(false);
        std::string elabel = e.get("label").as_string();
        svg << "<line x1=\"" << sx << "\" y1=\"" << sy << "\" x2=\"" << ex << "\" y2=\"" << ey
            << "\" stroke=\"#475569\" stroke-width=\"1.8\" marker-end=\"url(#omni-arrow)\""
            << (dashed ? " stroke-dasharray=\"5 3\"" : "") << "/>";
        if (!elabel.empty()) {
            double mx = (sx + ex) * 0.5, my = (sy + ey) * 0.5 - 5;
            svg << "<rect x=\"" << (mx - elabel.size() * 4 - 4) << "\" y=\"" << (my - 10)
                << "\" width=\"" << (elabel.size() * 8 + 8) << "\" height=\"15\" rx=\"4\" fill=\"#FFFFFF\" opacity=\"0.9\"/>";
            svg << "<text x=\"" << mx << "\" y=\"" << my << "\" text-anchor=\"middle\" font-size=\"10.5\" font-weight=\"600\" fill=\"#334155\">"
                << html_escape(elabel) << "</text>";
        }
    }

    // Draw nodes
    for (const auto& n : nodes) {
        std::string shape = n.get("shape").as_string("rect");
        if (shape == "frame") continue;
        double x = n.get("x").as_double(40), y = n.get("y").as_double(40);
        double w = n.get("w").as_double(130), h = n.get("h").as_double(50);
        std::string fill = n.get("fill").as_string("#FFFFFF");
        std::string stroke = n.get("stroke").as_string("#2563EB");
        std::string label = n.get("label").as_string("步骤");
        std::string sub = n.get("sub").as_string("");

        if (shape == "pill") {
            svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
                << "\" rx=\"" << (h * 0.5) << "\" fill=\"" << html_escape(fill) << "\" stroke=\"" << html_escape(stroke) << "\" stroke-width=\"1.8\"/>";
        } else if (shape == "diamond") {
            double cx = x + w * 0.5, cy = y + h * 0.5;
            svg << "<polygon points=\"" << cx << "," << y << " " << (x + w) << "," << cy << " "
                << cx << "," << (y + h) << " " << x << "," << cy << "\" fill=\"" << html_escape(fill)
                << "\" stroke=\"" << html_escape(stroke) << "\" stroke-width=\"1.8\"/>";
        } else if (shape == "io") {
            double sk = 12;
            svg << "<polygon points=\"" << (x + sk) << "," << y << " " << (x + w) << "," << y << " "
                << (x + w - sk) << "," << (y + h) << " " << x << "," << (y + h) << "\" fill=\"" << html_escape(fill)
                << "\" stroke=\"" << html_escape(stroke) << "\" stroke-width=\"1.8\"/>";
        } else {
            svg << "<rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
                << "\" rx=\"8\" fill=\"" << html_escape(fill) << "\" stroke=\"" << html_escape(stroke) << "\" stroke-width=\"1.8\"/>";
        }

        double cx = x + w * 0.5, cy = y + h * 0.5;
        if (sub.empty()) {
            svg << "<text x=\"" << cx << "\" y=\"" << (cy + 4)
                << "\" text-anchor=\"middle\" font-size=\"12\" font-weight=\"600\" font-family=\"'黑体','Microsoft YaHei',sans-serif\" fill=\"#0F172A\">"
                << html_escape(label) << "</text>";
        } else {
            svg << "<text x=\"" << cx << "\" y=\"" << (cy - 3)
                << "\" text-anchor=\"middle\" font-size=\"11.5\" font-weight=\"700\" font-family=\"'黑体','Microsoft YaHei',sans-serif\" fill=\"#0F172A\">"
                << html_escape(label) << "</text>";
            svg << "<text x=\"" << cx << "\" y=\"" << (cy + 13)
                << "\" text-anchor=\"middle\" font-size=\"10\" font-family=\"Consolas,'宋体',monospace\" fill=\"#475569\">"
                << html_escape(sub) << "</text>";
        }
    }

    svg << "</svg>";
    return svg.str();
}

std::string format_block_props_directive(const BlockProps& props) {
    if (props.empty()) return "";
    std::ostringstream oss;
    oss << "<!-- wp:";
    bool first = true;
    auto push_kv = [&](const std::string& k, const std::string& v) {
        if (!first) oss << ";";
        oss << k << "=" << v;
        first = false;
    };
    if (!props.align.empty()) push_kv("align", props.align);
    if (!props.line_rule.empty() && (props.line_rule == "atLeast" || props.line_rule == "exact") && props.line_pt > 0.0) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%s:%.1fpt", props.line_rule == "atLeast" ? "min" : "exact", props.line_pt);
        push_kv("line", buf);
    } else if (props.line_spacing > 0.0) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.2f", props.line_spacing);
        push_kv("line", buf);
    }
    if (props.space_before_pt >= 0.0) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1f", props.space_before_pt);
        push_kv("before", buf);
    }
    if (props.space_after_pt >= 0.0) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1f", props.space_after_pt);
        push_kv("after", buf);
    }
    if (props.first_line_indent_chars > 0.0) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1f", props.first_line_indent_chars);
        push_kv("indent", buf);
    }
    if (!props.font_family.empty()) push_kv("font", props.font_family);
    if (props.font_size_pt > 0.0) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1f", props.font_size_pt);
        push_kv("size", buf);
    }
    if (props.has_border) push_kv("border", "1");
    if (!props.shading.empty()) push_kv("bg", props.shading);
    oss << " -->";
    return oss.str();
}

bool parse_block_props_directive(const std::string& line, BlockProps& out_props) {
    std::string t = trim(line);
    if (t.rfind("<!--", 0) != 0 || t.size() < 11) return false;
    size_t wp = t.find("wp:");
    size_t end = t.rfind("-->");
    if (wp == std::string::npos || end == std::string::npos || end <= wp + 3) return false;
    std::string body = trim(t.substr(wp + 3, end - (wp + 3)));
    std::istringstream iss(body);
    std::string item;
    while (std::getline(iss, item, ';')) {
        size_t eq = item.find('=');
        if (eq == std::string::npos) continue;
        std::string k = to_lower_ascii(trim(item.substr(0, eq)));
        std::string v = trim(item.substr(eq + 1));
        if (v.empty()) continue;
        try {
            if (k == "align") out_props.align = to_lower_ascii(v);
            else if (k == "line") {
                std::string lv = to_lower_ascii(v);
                if (lv.rfind("min:", 0) == 0) {
                    out_props.line_rule = "atLeast";
                    out_props.line_pt = std::stod(lv.substr(4));
                } else if (lv.rfind("exact:", 0) == 0) {
                    out_props.line_rule = "exact";
                    out_props.line_pt = std::stod(lv.substr(6));
                } else {
                    out_props.line_rule = "auto";
                    out_props.line_spacing = std::stod(lv);
                }
            }
            else if (k == "before") out_props.space_before_pt = std::stod(v);
            else if (k == "after") out_props.space_after_pt = std::stod(v);
            else if (k == "indent") out_props.first_line_indent_chars = std::stod(v);
            else if (k == "font") out_props.font_family = v;
            else if (k == "size") out_props.font_size_pt = std::stod(v);
            else if (k == "border") out_props.has_border = (v == "1" || v == "true");
            else if (k == "bg") out_props.shading = v;
        } catch (...) {}
    }
    return true;
}

std::vector<InlineSpan> parse_inline_markdown(const std::string& text) {
    std::vector<InlineSpan> spans;
    InlineSpan base;
    parse_inline_rec(text, base, spans);
    return spans;
}

MdDocument parse_markdown(const std::string& md) {
    MdDocument doc;
    auto lines = split_lines(md);
    const size_t n = lines.size();

    BlockProps pending_props;
    bool has_pending_props = false;

    auto consume_props = [&](MdBlock& blk) {
        if (has_pending_props) {
            blk.props = pending_props;
            pending_props = BlockProps{};
            has_pending_props = false;
        }
    };

    size_t i = 0;
    while (i < n) {
        std::string raw_line = lines[i];
        std::string t = trim(raw_line);

        if (t.empty()) {
            ++i;
            continue;
        }

        // Explicit Word PageBreak directive <!-- pagebreak --> or \newpage
        if (to_lower_ascii(t) == "<!-- pagebreak -->" || t == "\\newpage") {
            MdBlock blk;
            blk.kind = MdBlock::Kind::PageBreak;
            doc.blocks.push_back(std::move(blk));
            ++i;
            continue;
        }

        // Check <!-- wp:... --> block directive
        BlockProps parsed_dir;
        if (parse_block_props_directive(t, parsed_dir)) {
            pending_props = parsed_dir;
            has_pending_props = true;
            ++i;
            continue;
        }

        // Standalone HTML block <p style="..." align="...">inner</p>
        if (to_lower_ascii(t).rfind("<p", 0) == 0 && t.find('>') != std::string::npos) {
            size_t gt = t.find('>');
            size_t close_p = to_lower_ascii(t).rfind("</p>");
            if (close_p != std::string::npos && close_p > gt) {
                std::string attrs = t.substr(2, gt - 2);
                std::string inner = trim(t.substr(gt + 1, close_p - (gt + 1)));
                MdBlock blk;
                blk.kind = MdBlock::Kind::Paragraph;
                consume_props(blk);
                std::string al = extract_attr_value(attrs, "align");
                if (!al.empty()) blk.props.align = to_lower_ascii(al);
                std::string st = extract_attr_value(attrs, "style");
                if (!st.empty()) {
                    InlineSpan dummy;
                    apply_css_style(st, dummy);
                    if (!dummy.font_family.empty()) blk.props.font_family = dummy.font_family;
                    if (dummy.font_size_pt > 0.0) blk.props.font_size_pt = dummy.font_size_pt;
                    if (!dummy.highlight.empty()) blk.props.shading = dummy.highlight;
                }
                blk.raw = inner;
                blk.inlines = parse_inline_markdown(inner);
                doc.blocks.push_back(std::move(blk));
                ++i;
                continue;
            }
        }

        // Fenced code block ``` or ```figma diagram block
        if (t.rfind("```", 0) == 0) {
            std::string lang = trim(t.substr(3));
            ++i;
            std::ostringstream oss;
            bool first = true;
            while (i < n) {
                if (trim(lines[i]).rfind("```", 0) == 0) {
                    ++i;
                    break;
                }
                if (!first) oss << "\n";
                oss << lines[i];
                first = false;
                ++i;
            }
            MdBlock blk;
            if (to_lower_ascii(lang) == "figma") {
                blk.kind = MdBlock::Kind::DiagramBlock;
                blk.lang = "figma";
            } else {
                blk.kind = MdBlock::Kind::CodeBlock;
                blk.lang = lang;
            }
            blk.raw = oss.str();
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            continue;
        }

        // Display math block $$...$$
        if (t.rfind("$$", 0) == 0) {
            MdBlock blk;
            blk.kind = MdBlock::Kind::MathBlock;
            if (t.size() > 4 && t.rfind("$$") == t.size() - 2) {
                blk.raw = trim(t.substr(2, t.size() - 4));
                ++i;
            } else {
                std::ostringstream oss;
                std::string first_part = trim(t.substr(2));
                if (!first_part.empty()) oss << first_part;
                ++i;
                while (i < n) {
                    std::string lt = trim(lines[i]);
                    if (lt == "$$" || (lt.size() >= 2 && lt.rfind("$$") == lt.size() - 2)) {
                        if (lt.size() > 2) {
                            if (oss.tellp() > 0) oss << "\n";
                            oss << trim(lt.substr(0, lt.size() - 2));
                        }
                        ++i;
                        break;
                    }
                    if (oss.tellp() > 0) oss << "\n";
                    oss << lines[i];
                    ++i;
                }
                blk.raw = oss.str();
            }
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            continue;
        }

        // Heading #..######
        if (t[0] == '#') {
            int lvl = 0;
            while (lvl < 6 && static_cast<size_t>(lvl) < t.size() && t[static_cast<size_t>(lvl)] == '#') {
                ++lvl;
            }
            if (static_cast<size_t>(lvl) < t.size() && t[static_cast<size_t>(lvl)] == ' ') {
                MdBlock blk;
                blk.kind = MdBlock::Kind::Heading;
                blk.level = lvl;
                blk.raw = trim(t.substr(static_cast<size_t>(lvl) + 1));
                blk.inlines = parse_inline_markdown(blk.raw);
                consume_props(blk);
                doc.blocks.push_back(std::move(blk));
                ++i;
                continue;
            }
        }

        // Horizontal rule
        if (is_hr_line(t)) {
            MdBlock blk;
            blk.kind = MdBlock::Kind::Hr;
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            ++i;
            continue;
        }

        // Table (header + separator row)
        std::vector<std::string> aligns;
        if (t.find('|') != std::string::npos && i + 1 < n && is_table_separator(trim(lines[i + 1]), aligns)) {
            MdBlock blk;
            blk.kind = MdBlock::Kind::Table;
            blk.table_aligns = std::move(aligns);

            auto header_cells = split_table_cells(t);
            std::vector<std::vector<InlineSpan>> header_row;
            for (const auto& c : header_cells) header_row.push_back(parse_inline_markdown(c));
            blk.table_rows.push_back(std::move(header_row));

            i += 2;
            while (i < n) {
                std::string row_t = trim(lines[i]);
                if (row_t.empty() || row_t.find('|') == std::string::npos) break;
                auto row_cells = split_table_cells(row_t);
                std::vector<std::vector<InlineSpan>> parsed_row;
                for (const auto& c : row_cells) parsed_row.push_back(parse_inline_markdown(c));
                blk.table_rows.push_back(std::move(parsed_row));
                ++i;
            }
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            continue;
        }

        // Blockquote
        if (t[0] == '>') {
            std::string quote_text = trim(t.substr(1));
            ++i;
            while (i < n) {
                std::string nt = trim(lines[i]);
                if (nt.empty() || nt[0] != '>') break;
                if (!quote_text.empty()) quote_text += "\n";
                quote_text += trim(nt.substr(1));
                ++i;
            }
            // Replace internal newlines inside quote with <br> so paragraphs don't smash lines together
            std::string q_with_br;
            for (size_t k = 0; k < quote_text.size(); ++k) {
                if (quote_text[k] == '\n') q_with_br += "<br/>";
                else q_with_br.push_back(quote_text[k]);
            }
            MdBlock blk;
            blk.kind = MdBlock::Kind::Blockquote;
            blk.raw = quote_text;
            blk.inlines = parse_inline_markdown(q_with_br);
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            continue;
        }

        int indent_sp = leading_indent_spaces(raw_line);
        int list_lvl = std::max(0, std::min(5, indent_sp / 2));

        // Task list item - [ ] / - [x]
        bool task_checked = false;
        std::string task_rest;
        if (parse_task_prefix(t, task_checked, task_rest)) {
            MdBlock blk;
            blk.kind = MdBlock::Kind::TaskItem;
            blk.indent_level = list_lvl;
            blk.checked = task_checked;
            blk.raw = task_rest;
            blk.inlines = parse_inline_markdown(task_rest);
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            ++i;
            continue;
        }

        // Unordered list item
        if ((t.size() >= 2 && (t[0] == '-' || t[0] == '*' || t[0] == '+') && t[1] == ' ')) {
            MdBlock blk;
            blk.kind = MdBlock::Kind::BulletItem;
            blk.indent_level = list_lvl;
            blk.raw = trim(t.substr(2));
            blk.inlines = parse_inline_markdown(blk.raw);
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            ++i;
            continue;
        }

        // Ordered list item
        int ord_num = 1;
        std::string ord_rest;
        if (parse_ordered_prefix(t, ord_num, ord_rest)) {
            MdBlock blk;
            blk.kind = MdBlock::Kind::OrderedItem;
            blk.level = ord_num;
            blk.indent_level = list_lvl;
            blk.raw = ord_rest;
            blk.inlines = parse_inline_markdown(ord_rest);
            consume_props(blk);
            doc.blocks.push_back(std::move(blk));
            ++i;
            continue;
        }

        // Paragraph (respect two-trailing-space / trailing backslash soft line breaks!)
        auto has_hard_break = [](const std::string& ln) -> bool {
            if (ln.size() >= 2 && ln[ln.size() - 1] == ' ' && ln[ln.size() - 2] == ' ') return true;
            if (!ln.empty() && ln.back() == '\\') return true;
            return false;
        };

        std::string para = t;
        bool prev_break = has_hard_break(raw_line);
        if (prev_break && !para.empty() && para.back() == '\\') para.pop_back();
        ++i;
        while (i < n) {
            std::string nraw = lines[i];
            std::string nt = trim(nraw);
            BlockProps tmp_p;
            if (nt.empty() || nt[0] == '#' || nt[0] == '>' || nt.rfind("```", 0) == 0 ||
                nt.rfind("$$", 0) == 0 || is_hr_line(nt) ||
                parse_block_props_directive(nt, tmp_p) ||
                to_lower_ascii(nt).rfind("<p", 0) == 0 ||
                (nt.size() >= 2 && (nt[0] == '-' || nt[0] == '*' || nt[0] == '+') && nt[1] == ' ') ||
                parse_ordered_prefix(nt, ord_num, ord_rest)) {
                break;
            }
            if (nt.find('|') != std::string::npos && i + 1 < n && is_table_separator(trim(lines[i + 1]), aligns)) {
                break;
            }
            if (prev_break) para += "<br/>" + nt;
            else para += " " + nt;
            prev_break = has_hard_break(nraw);
            if (prev_break && !para.empty() && para.back() == '\\') para.pop_back();
            ++i;
        }
        MdBlock blk;
        blk.kind = MdBlock::Kind::Paragraph;
        blk.raw = para;
        blk.inlines = parse_inline_markdown(para);
        consume_props(blk);
        doc.blocks.push_back(std::move(blk));
    }

    return doc;
}

std::string render_markdown_to_html(const MdDocument& doc,
                                    bool standalone_a4,
                                    const std::string& title,
                                    const std::string& asset_doc_path) {
    std::ostringstream body;
    struct ListState {
        MdBlock::Kind kind;
        int indent_level;
    };
    std::vector<ListState> list_stack;

    auto tag_for = [](MdBlock::Kind k) -> const char* {
        return k == MdBlock::Kind::OrderedItem ? "ol" : "ul";
    };

    auto close_lists_to = [&](int target_indent, MdBlock::Kind target_kind, bool force_close_all) {
        if (force_close_all) {
            while (!list_stack.empty()) {
                body << "</" << tag_for(list_stack.back().kind) << ">\n";
                list_stack.pop_back();
            }
            return;
        }
        while (!list_stack.empty() && list_stack.back().indent_level > target_indent) {
            body << "</" << tag_for(list_stack.back().kind) << ">\n";
            list_stack.pop_back();
        }
        if (!list_stack.empty() && list_stack.back().indent_level == target_indent &&
            list_stack.back().kind != target_kind) {
            body << "</" << tag_for(list_stack.back().kind) << ">\n";
            list_stack.pop_back();
        }
        if (list_stack.empty() || list_stack.back().indent_level < target_indent) {
            std::string cls = (target_kind == MdBlock::Kind::OrderedItem)
                                  ? "omni-ol"
                                  : (target_kind == MdBlock::Kind::TaskItem ? "omni-ul omni-task-list" : "omni-ul");
            body << "<" << tag_for(target_kind) << " class=\"" << cls << "\" data-indent=\"" << target_indent << "\">\n";
            list_stack.push_back({target_kind, target_indent});
        }
    };

    for (const auto& blk : doc.blocks) {
        bool is_list = (blk.kind == MdBlock::Kind::BulletItem ||
                        blk.kind == MdBlock::Kind::OrderedItem ||
                        blk.kind == MdBlock::Kind::TaskItem);
        if (!is_list) {
            close_lists_to(0, blk.kind, true);
        }

        std::string wp_attrs = build_block_style_attrs(blk.props);

        switch (blk.kind) {
            case MdBlock::Kind::Heading:
                body << "<h" << blk.level << " class=\"omni-h" << blk.level << "\"" << wp_attrs << ">"
                     << render_inlines_html(blk.inlines, standalone_a4, asset_doc_path)
                     << "</h" << blk.level << ">\n";
                break;
            case MdBlock::Kind::Paragraph:
                body << "<p class=\"omni-p\"" << wp_attrs << ">"
                     << render_inlines_html(blk.inlines, standalone_a4, asset_doc_path)
                     << "</p>\n";
                break;
            case MdBlock::Kind::Blockquote:
                body << "<blockquote class=\"omni-quote\"" << wp_attrs << ">"
                     << render_inlines_html(blk.inlines, standalone_a4, asset_doc_path)
                     << "</blockquote>\n";
                break;
            case MdBlock::Kind::BulletItem:
                close_lists_to(blk.indent_level, blk.kind, false);
                body << "<li" << wp_attrs << ">" << render_inlines_html(blk.inlines, standalone_a4, asset_doc_path) << "</li>\n";
                break;
            case MdBlock::Kind::OrderedItem:
                close_lists_to(blk.indent_level, blk.kind, false);
                body << "<li value=\"" << blk.level << "\"" << wp_attrs << ">"
                     << render_inlines_html(blk.inlines, standalone_a4, asset_doc_path) << "</li>\n";
                break;
            case MdBlock::Kind::TaskItem:
                close_lists_to(blk.indent_level, blk.kind, false);
                body << "<li class=\"omni-task-li\" data-checked=\"" << (blk.checked ? "1" : "0") << "\"" << wp_attrs << ">"
                     << "<input type=\"checkbox\" class=\"omni-task-cb\"" << (blk.checked ? " checked" : "")
                     << " onclick=\"toggleWordTaskItem(this)\"/> "
                     << render_inlines_html(blk.inlines, standalone_a4, asset_doc_path) << "</li>\n";
                break;
            case MdBlock::Kind::CodeBlock: {
                std::string lang_disp = blk.lang.empty() ? "code" : blk.lang;
                body << "<div class=\"omni-code-box\" contenteditable=\"false\" data-lang=\"" << html_escape(blk.lang)
                     << "\" data-code=\"" << html_escape(blk.raw) << "\"" << wp_attrs << ">"
                     << "<div class=\"omni-code-head\"><span class=\"omni-code-lang mono\">" << html_escape(lang_disp) << "</span>"
                     << (!standalone_a4 ? "<span class=\"omni-box-actions\"><button type=\"button\" class=\"omni-box-btn\" onclick=\"editCodeBoxBlock(this)\">编辑代码</button><button type=\"button\" class=\"omni-box-btn\" onclick=\"deleteAtomicBlock(this)\">删除</button></span>" : "")
                     << "</div>"
                     << "<pre class=\"omni-pre\"><code>" << html_escape(blk.raw) << "</code></pre>"
                     << "</div>\n";
                break;
            }
            case MdBlock::Kind::MathBlock:
                body << "<div class=\"omni-math-block\" contenteditable=\"false\" data-tex=\"" << html_escape(blk.raw) << "\"" << wp_attrs << ">"
                     << "<div class=\"omni-math-rendered\">" << render_tex_to_html(blk.raw, true) << "</div>"
                     << "<div class=\"omni-math-tag\">LaTeX / Word OMML 公式"
                     << (!standalone_a4 ? " · <button type=\"button\" class=\"omni-box-btn\" onclick=\"editFormulaBlock(this)\">双击/点此编辑公式</button> <button type=\"button\" class=\"omni-box-btn\" onclick=\"deleteAtomicBlock(this)\">删除</button>" : "")
                     << "</div>"
                     << "</div>\n";
                break;
            case MdBlock::Kind::DiagramBlock:
                body << "<div class=\"omni-figma-block\" contenteditable=\"false\" data-figma-json=\"" << html_escape(blk.raw) << "\"" << wp_attrs << ">"
                     << (!standalone_a4 ? "<div class=\"omni-figma-head\"><span>❖ Figma 算法可视化图框</span><span class=\"omni-box-actions\"><button type=\"button\" class=\"omni-box-btn\" onclick=\"editFigmaBlock(this)\">在画布中编辑</button><button type=\"button\" class=\"omni-box-btn\" onclick=\"deleteAtomicBlock(this)\">删除</button></span></div>" : "")
                     << "<div class=\"omni-figma-canvas\">" << render_figma_json_to_svg(blk.raw) << "</div>"
                     << "</div>\n";
                break;
            case MdBlock::Kind::Hr:
                body << "<hr class=\"omni-hr\" />\n";
                break;
            case MdBlock::Kind::PageBreak:
                if (standalone_a4) {
                    body << "<div style=\"page-break-after:always;break-after:page;height:0;overflow:hidden;\"></div>\n";
                } else {
                    body << "<div class=\"omni-pagebreak\" contenteditable=\"false\" data-omni-pagebreak=\"1\"><span>—— 分页符 (Page Break) ——</span><button type=\"button\" class=\"omni-box-btn\" onclick=\"deleteAtomicBlock(this)\">删除</button></div>\n";
                }
                break;
            case MdBlock::Kind::Table: {
                body << "<table class=\"omni-table\">\n";
                for (size_t r = 0; r < blk.table_rows.size(); ++r) {
                    const char* cell_tag = (r == 0) ? "th" : "td";
                    if (r == 0) body << "<thead>\n";
                    else if (r == 1) body << "<tbody>\n";
                    body << "<tr>";
                    for (size_t c = 0; c < blk.table_rows[r].size(); ++c) {
                        std::string align = (c < blk.table_aligns.size()) ? blk.table_aligns[c] : "left";
                        body << "<" << cell_tag << " data-align=\"" << align << "\" style=\"text-align:" << align << "\">"
                             << render_inlines_html(blk.table_rows[r][c], standalone_a4, asset_doc_path)
                             << "</" << cell_tag << ">";
                    }
                    body << "</tr>\n";
                    if (r == 0) body << "</thead>\n";
                }
                if (blk.table_rows.size() > 1) body << "</tbody>\n";
                body << "</table>\n";
                break;
            }
        }
    }
    close_lists_to(0, MdBlock::Kind::Paragraph, true);

    if (!standalone_a4) {
        return body.str();
    }

    std::ostringstream doc_html;
    doc_html << "<!DOCTYPE html>\n<html lang=\"zh-CN\">\n<head>\n"
             << "<meta charset=\"UTF-8\" />\n"
             << "<title>" << html_escape(title) << "</title>\n"
             << "<style>\n"
             << "@page { size: A4; margin: 25.4mm 25.4mm; }\n"
             << "body { font-family: 'Times New Roman', '宋体', 'SimSun', serif; color: #111827; line-height: 1.5; font-size: 10.5pt; margin: 0; padding: 0; }\n"
             << "h1.omni-h1, h2.omni-h2, h3.omni-h3, h4.omni-h4, h5.omni-h5, h6.omni-h6 { font-family: 'Times New Roman', '黑体', 'SimHei', sans-serif; font-weight: 700; }\n"
             << "h1.omni-h1 { font-size: 22pt; margin: 0 0 10pt 0; border-bottom: 1.5px solid #E5E7EB; padding-bottom: 6pt; }\n"
             << "h2.omni-h2 { font-size: 16pt; margin: 14pt 0 6pt 0; }\n"
             << "h3.omni-h3 { font-size: 14pt; margin: 12pt 0 4pt 0; }\n"
             << "h4.omni-h4 { font-size: 12pt; margin: 10pt 0 4pt 0; }\n"
             << "h5.omni-h5 { font-size: 11pt; margin: 8pt 0 3pt 0; }\n"
             << "h6.omni-h6 { font-size: 10.5pt; margin: 8pt 0 3pt 0; }\n"
             << "p.omni-p { margin: 5pt 0; }\n"
             << "blockquote.omni-quote { font-family: 'Times New Roman', '楷体', 'KaiTi', serif; margin: 8pt 0; padding: 6pt 12pt; border-left: 3px solid #3B82F6; background: #F8FAFC; color: #334155; }\n"
             << "ul.omni-ul, ol.omni-ol { margin: 5pt 0 7pt 18pt; padding: 0; }\n"
             << "ul.omni-task-list { list-style: none; margin-left: 4pt; }\n"
             << "li { margin: 2.5pt 0; }\n"
             << "code.omni-inline-code { font-family: 'Consolas', monospace; background: #F1F5F9; padding: 1pt 4pt; border-radius: 3pt; font-size: 9.5pt; }\n"
             << ".omni-code-box { margin: 8pt 0; border: 1px solid #CBD5E1; border-radius: 6pt; overflow: hidden; background: #F8FAFC; }\n"
             << ".omni-code-head { padding: 3pt 8pt; background: #E2E8F0; font-size: 8.5pt; font-weight: 700; color: #334155; font-family: Consolas, monospace; }\n"
             << "pre.omni-pre { font-family: 'Consolas', monospace; margin: 0; padding: 9pt; font-size: 9pt; white-space: pre-wrap; }\n"
             << ".omni-math-block { margin: 10pt 0; padding: 10pt 14pt; background: #F8FAFC; border: 1px solid #E2E8F0; border-radius: 6pt; text-align: center; }\n"
             << ".omni-math-rendered, .omni-inline-math { font-family: 'Cambria Math', 'Times New Roman', serif; }\n"
             << ".omni-math-rendered { font-size: 12pt; }\n"
             << ".omni-math-tag { font-size: 8pt; color: #64748B; margin-top: 4pt; font-family: monospace; }\n"
             << ".omni-mfrac { display: inline-flex; flex-direction: column; vertical-align: middle; text-align: center; margin: 0 2px; font-size: 0.9em; line-height: 1.15; }\n"
             << ".omni-mnum { border-bottom: 1px solid currentColor; padding: 0 3px 1px 3px; }\n"
             << ".omni-mden { padding: 1px 3px 0 3px; }\n"
             << ".omni-msqrt { display: inline-flex; align-items: baseline; }\n"
             << ".omni-mrad-body { border-top: 1px solid currentColor; padding: 0 2px; }\n"
             << ".omni-mmatrix-wrap { display: inline-flex; align-items: center; vertical-align: middle; gap: 2px; }\n"
             << ".omni-mdelim { font-size: 1.5em; font-weight: 300; line-height: 1; }\n"
             << ".omni-mmatrix { display: inline-table; border-collapse: collapse; }\n"
             << ".omni-mrow { display: table-row; }\n"
             << ".omni-mcell { display: table-cell; padding: 2px 6px; text-align: center; }\n"
             << ".omni-mnary { display: inline-flex; align-items: center; vertical-align: middle; gap: 2px; }\n"
             << ".omni-mnary-op { font-size: 1.3em; }\n"
             << ".omni-mnary-lims, .omni-msupsub { display: inline-flex; flex-direction: column; font-size: 0.72em; line-height: 1.05; vertical-align: middle; }\n"
             << ".omni-maccent { position: relative; display: inline-block; }\n"
             << ".omni-maccent-mk { position: absolute; top: -0.55em; left: 0; right: 0; text-align: center; font-size: 0.7em; }\n"
             << ".omni-mover { border-top: 1px solid currentColor; display: inline-block; line-height: 1.1; }\n"
             << ".omni-mvar { font-style: italic; }\n"
             << ".omni-mtext { font-style: normal; font-family: 'Times New Roman', '宋体', serif; }\n"
             << ".omni-figma-block { margin: 10pt 0; border: 1px solid #CBD5E1; border-radius: 8pt; overflow: hidden; background: #FFFFFF; }\n"
             << ".omni-img { max-width: 100%; height: auto; display: inline-block; }\n"
             << "table.omni-table { width: 100%; border-collapse: collapse; margin: 10pt 0; font-size: 10pt; }\n"
             << "table.omni-table th, table.omni-table td { border: 1px solid #CBD5E1; padding: 6pt 8pt; }\n"
             << "table.omni-table th { background: #F1F5F9; font-weight: 600; }\n"
             << ".omni-wikilink { border: none; background: #EFF6FF; color: #1D4ED8; padding: 1pt 4pt; border-radius: 3pt; font-weight: 600; }\n"
             << ".omni-link { color: #2563EB; text-decoration: underline; }\n"
             << "hr.omni-hr { border: none; border-top: 1px solid #CBD5E1; margin: 12pt 0; }\n"
             << "</style>\n</head>\n<body>\n"
             << body.str()
             << "</body>\n</html>\n";
    return doc_html.str();
}

std::vector<std::string> extract_latex_blocks(const std::string& md) {
    std::vector<std::string> formulas;
    MdDocument doc = parse_markdown(md);
    for (const auto& blk : doc.blocks) {
        if (blk.kind == MdBlock::Kind::MathBlock && !blk.raw.empty()) {
            formulas.push_back(blk.raw);
        } else {
            for (const auto& sp : blk.inlines) {
                if (sp.kind == InlineSpan::Kind::InlineMath && !sp.text.empty()) {
                    formulas.push_back(sp.text);
                }
            }
        }
    }
    return formulas;
}

} // namespace omni
