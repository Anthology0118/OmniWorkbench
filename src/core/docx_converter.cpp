#include "docx_converter.hpp"

#include "json.hpp"
#include "markdown.hpp"
#include "platform_utils.hpp"
#include "xml_doc.hpp"
#include "zip_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>
#include <zlib.h>

namespace omni {

namespace fs = std::filesystem;

namespace {

std::string to_lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

std::string to_upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::string trim_ws(const std::string& s) {
    size_t a = 0;
    while (a < s.size() && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    size_t b = s.size();
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

std::string normalize_hex(const std::string& c) {
    std::string s = trim_ws(c);
    if (!s.empty() && s[0] == '#') s = s.substr(1);
    if (s.size() == 3) {
        std::string r;
        r.push_back(s[0]); r.push_back(s[0]);
        r.push_back(s[1]); r.push_back(s[1]);
        r.push_back(s[2]); r.push_back(s[2]);
        return to_upper(r);
    }
    if (s.size() == 6) {
        for (char ch : s) {
            if (!std::isxdigit(static_cast<unsigned char>(ch))) return "";
        }
        return to_upper(s);
    }
    // Map common CSS color names
    std::string low = to_lower(s);
    if (low == "yellow") return "FEF08A";
    if (low == "green") return "16A34A";
    if (low == "red") return "DC2626";
    if (low == "blue") return "2563EB";
    if (low == "cyan") return "A5F3FC";
    if (low == "pink" || low == "magenta") return "FBCFE8";
    if (low == "purple") return "7C3AED";
    if (low == "orange") return "EA580C";
    if (low == "gray" || low == "grey") return "64748B";
    return "";
}

std::string map_word_highlight_to_hex(const std::string& val) {
    std::string low = to_lower(val);
    if (low == "yellow") return "#FEF08A";
    if (low == "green") return "#BBF7D0";
    if (low == "cyan") return "#BAE6FD";
    if (low == "magenta") return "#FBCFE8";
    if (low == "blue") return "#BFDBFE";
    if (low == "red") return "#FECACA";
    if (low == "darkblue") return "#93C5FD";
    if (low == "darkyellow") return "#FDE047";
    if (low == "lightgray" || low == "darkgray") return "#E2E8F0";
    return "#FEF08A";
}

// ============================================================================
// Base64 & Image / PNG Helpers
// ============================================================================
std::string base64_encode(const std::string& in) {
    static const char tbl[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    int val = 0, valb = -6;
    for (unsigned char c : in) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(tbl[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(tbl[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

std::string base64_decode(const std::string& in) {
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

bool read_image_dimensions(const std::string& bytes, const std::string& ext, int& out_w, int& out_h) {
    out_w = 480;
    out_h = 280;
    const auto* d = reinterpret_cast<const uint8_t*>(bytes.data());
    size_t n = bytes.size();
    if (ext == "png" && n >= 24 && d[0] == 0x89 && d[1] == 'P' && d[2] == 'N' && d[3] == 'G') {
        out_w = static_cast<int>((d[16] << 24) | (d[17] << 16) | (d[18] << 8) | d[19]);
        out_h = static_cast<int>((d[20] << 24) | (d[21] << 16) | (d[22] << 8) | d[23]);
        return out_w > 0 && out_h > 0;
    }
    if ((ext == "jpg" || ext == "jpeg") && n >= 4 && d[0] == 0xFF && d[1] == 0xD8) {
        size_t i = 2;
        while (i + 9 < n) {
            if (d[i] != 0xFF) { ++i; continue; }
            uint8_t marker = d[i + 1];
            if (marker == 0xD8 || marker == 0xD9 || marker == 0x00) { i += 2; continue; }
            uint16_t seg_len = static_cast<uint16_t>((d[i + 2] << 8) | d[i + 3]);
            if (marker >= 0xC0 && marker <= 0xC3 && i + 8 < n) {
                out_h = static_cast<int>((d[i + 5] << 8) | d[i + 6]);
                out_w = static_cast<int>((d[i + 7] << 8) | d[i + 8]);
                return out_w > 0 && out_h > 0;
            }
            if (seg_len < 2) break;
            i += 2 + seg_len;
        }
    }
    if (ext == "gif" && n >= 10 && d[0] == 'G' && d[1] == 'I' && d[2] == 'F') {
        out_w = static_cast<int>(d[6] | (d[7] << 8));
        out_h = static_cast<int>(d[8] | (d[9] << 8));
        return out_w > 0 && out_h > 0;
    }
    if (ext == "bmp" && n >= 26 && d[0] == 'B' && d[1] == 'M') {
        out_w = static_cast<int>(d[18] | (d[19] << 8) | (d[20] << 16) | (d[21] << 24));
        out_h = std::abs(static_cast<int>(static_cast<int32_t>(d[22] | (d[23] << 8) | (d[24] << 16) | (d[25] << 24))));
        return out_w > 0 && out_h > 0;
    }
    return false;
}

void write_be32(std::string& out, uint32_t v) {
    out.push_back(static_cast<char>((v >> 24) & 0xFF));
    out.push_back(static_cast<char>((v >> 16) & 0xFF));
    out.push_back(static_cast<char>((v >> 8) & 0xFF));
    out.push_back(static_cast<char>(v & 0xFF));
}

void append_png_chunk(std::string& out, const char* type, const std::string& data) {
    write_be32(out, static_cast<uint32_t>(data.size()));
    out.append(type, 4);
    out.append(data);
    uLong crc = crc32(0L, Z_NULL, 0);
    crc = crc32(crc, reinterpret_cast<const Bytef*>(type), 4);
    if (!data.empty()) {
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data.data()), static_cast<uInt>(data.size()));
    }
    write_be32(out, static_cast<uint32_t>(crc));
}

// Fallback software rasterizer so even CLI / unit tests render Figma diagrams as valid PNGs
std::string render_figma_fallback_png(const std::string& json_str) {
    int W = 680, H = 360;
    JsonValue doc;
    if (JsonValue::parse(json_str, doc) && doc.type == JsonValue::Type::Object) {
        W = std::max(320, std::min(1200, static_cast<int>(doc.get("width").as_int(680))));
        H = std::max(200, std::min(900, static_cast<int>(doc.get("height").as_int(360))));
    }
    std::vector<uint8_t> rgba(static_cast<size_t>(W) * static_cast<size_t>(H) * 4, 255);
    auto set_px = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
        if (x < 0 || y < 0 || x >= W || y >= H) return;
        size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(W) + static_cast<size_t>(x)) * 4;
        rgba[idx] = r; rgba[idx + 1] = g; rgba[idx + 2] = b; rgba[idx + 3] = 255;
    };
    auto parse_rgb = [&](const std::string& hex, uint8_t& r, uint8_t& g, uint8_t& b) {
        std::string h = normalize_hex(hex);
        if (h.size() != 6) { r = 37; g = 99; b = 235; return; }
        r = static_cast<uint8_t>(std::stoi(h.substr(0, 2), nullptr, 16));
        g = static_cast<uint8_t>(std::stoi(h.substr(2, 2), nullptr, 16));
        b = static_cast<uint8_t>(std::stoi(h.substr(4, 2), nullptr, 16));
    };
    auto fill_rect = [&](int x0, int y0, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
        for (int y = y0; y < y0 + rh; ++y)
            for (int x = x0; x < x0 + rw; ++x) set_px(x, y, r, g, b);
    };
    auto stroke_rect = [&](int x0, int y0, int rw, int rh, uint8_t r, uint8_t g, uint8_t b) {
        for (int x = x0; x < x0 + rw; ++x) {
            set_px(x, y0, r, g, b); set_px(x, y0 + 1, r, g, b);
            set_px(x, y0 + rh - 1, r, g, b); set_px(x, y0 + rh - 2, r, g, b);
        }
        for (int y = y0; y < y0 + rh; ++y) {
            set_px(x0, y, r, g, b); set_px(x0 + 1, y, r, g, b);
            set_px(x0 + rw - 1, y, r, g, b); set_px(x0 + rw - 2, y, r, g, b);
        }
    };
    auto draw_line = [&](int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
        int steps = std::max(1, std::max(std::abs(x1 - x0), std::abs(y1 - y0)));
        for (int s = 0; s <= steps; ++s) {
            int x = x0 + (x1 - x0) * s / steps;
            int y = y0 + (y1 - y0) * s / steps;
            set_px(x, y, r, g, b);
            set_px(x + 1, y, r, g, b);
            set_px(x, y + 1, r, g, b);
        }
    };

    fill_rect(0, 0, W, H, 248, 250, 252);
    for (int y = 20; y < H; y += 20)
        for (int x = 20; x < W; x += 20) set_px(x, y, 203, 213, 225);
    stroke_rect(0, 0, W, H, 226, 232, 240);

    if (doc.type == JsonValue::Type::Object) {
        const auto& nodes = doc.get("nodes").arr_val;
        const auto& edges = doc.get("edges").arr_val;
        std::map<std::string, const JsonValue*> nmap;
        for (const auto& n : nodes) nmap[n.get("id").as_string()] = &n;
        for (const auto& e : edges) {
            auto it1 = nmap.find(e.get("from").as_string());
            auto it2 = nmap.find(e.get("to").as_string());
            if (it1 != nmap.end() && it2 != nmap.end()) {
                int x1 = static_cast<int>(it1->second->get("x").as_double() + it1->second->get("w").as_double(120) * 0.5);
                int y1 = static_cast<int>(it1->second->get("y").as_double() + it1->second->get("h").as_double(48) * 0.5);
                int x2 = static_cast<int>(it2->second->get("x").as_double() + it2->second->get("w").as_double(120) * 0.5);
                int y2 = static_cast<int>(it2->second->get("y").as_double() + it2->second->get("h").as_double(48) * 0.5);
                draw_line(x1, y1, x2, y2, 71, 85, 105);
            }
        }
        for (const auto& n : nodes) {
            int nx = static_cast<int>(n.get("x").as_double(40));
            int ny = static_cast<int>(n.get("y").as_double(40));
            int nw = static_cast<int>(n.get("w").as_double(120));
            int nh = static_cast<int>(n.get("h").as_double(48));
            uint8_t fr, fg, fb, sr, sg, sb;
            parse_rgb(n.get("fill").as_string("#FFFFFF"), fr, fg, fb);
            parse_rgb(n.get("stroke").as_string("#2563EB"), sr, sg, sb);
            fill_rect(nx, ny, nw, nh, fr, fg, fb);
            stroke_rect(nx, ny, nw, nh, sr, sg, sb);
        }
    }

    // Build scanlines with filter byte 0
    std::string raw;
    raw.reserve(static_cast<size_t>(H) * (static_cast<size_t>(W) * 4 + 1));
    for (int y = 0; y < H; ++y) {
        raw.push_back(0);
        raw.append(reinterpret_cast<const char*>(&rgba[static_cast<size_t>(y) * static_cast<size_t>(W) * 4]),
                   static_cast<size_t>(W) * 4);
    }
    uLongf comp_bound = compressBound(static_cast<uLong>(raw.size()));
    std::string comp(comp_bound, '\0');
    compress2(reinterpret_cast<Bytef*>(&comp[0]), &comp_bound,
              reinterpret_cast<const Bytef*>(raw.data()), static_cast<uLong>(raw.size()), Z_BEST_SPEED);
    comp.resize(comp_bound);

    std::string png = "\x89PNG\r\n\x1a\n";
    std::string ihdr;
    write_be32(ihdr, static_cast<uint32_t>(W));
    write_be32(ihdr, static_cast<uint32_t>(H));
    ihdr.push_back(8); ihdr.push_back(6); ihdr.push_back(0); ihdr.push_back(0); ihdr.push_back(0);
    append_png_chunk(png, "IHDR", ihdr);
    append_png_chunk(png, "IDAT", comp);
    append_png_chunk(png, "IEND", "");
    return png;
}

// ============================================================================
// Rich TeX -> Word OMML (<m:oMath>) translator (fractions, roots, matrices, nary, accents)
// ============================================================================
class TexToOmml {
public:
    explicit TexToOmml(const std::string& tex) : s_(tex), pos_(0) {}

    std::string convert() {
        std::string inner = parse_sequence(0);
        return "<m:oMath>" + inner + "</m:oMath>";
    }

private:
    const std::string& s_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    static std::string make_mr(const std::string& text, bool plain = false) {
        if (text.empty()) return "";
        std::string out = "<m:r>";
        if (plain) {
            out += "<m:rPr><m:sty m:val=\"p\"/></m:rPr>";
        }
        out += "<w:rPr><w:rFonts w:ascii=\"Cambria Math\" w:hAnsi=\"Cambria Math\"/></w:rPr>";
        out += "<m:t xml:space=\"preserve\">" + xml_escape(text) + "</m:t></m:r>";
        return out;
    }

    static std::string map_symbol(const std::string& cmd) {
        static const std::map<std::string, std::string> syms = {
            {"alpha", "α"}, {"beta", "β"}, {"gamma", "γ"}, {"delta", "δ"},
            {"epsilon", "ϵ"}, {"varepsilon", "ε"}, {"zeta", "ζ"}, {"eta", "η"},
            {"theta", "θ"}, {"iota", "ι"}, {"kappa", "κ"}, {"lambda", "λ"},
            {"mu", "μ"}, {"nu", "ν"}, {"xi", "ξ"}, {"pi", "π"},
            {"rho", "ρ"}, {"sigma", "σ"}, {"tau", "τ"}, {"upsilon", "υ"},
            {"phi", "φ"}, {"varphi", "ϕ"}, {"chi", "χ"}, {"psi", "ψ"}, {"omega", "ω"},
            {"Gamma", "Γ"}, {"Delta", "Δ"}, {"Theta", "Θ"}, {"Lambda", "Λ"},
            {"Xi", "Ξ"}, {"Pi", "Π"}, {"Sigma", "Σ"}, {"Phi", "Φ"}, {"Psi", "Ψ"}, {"Omega", "Ω"},
            {"infty", "∞"}, {"partial", "∂"}, {"nabla", "∇"}, {"cdot", "·"},
            {"times", "×"}, {"div", "÷"}, {"pm", "±"}, {"mp", "∓"},
            {"leq", "≤"}, {"le", "≤"}, {"geq", "≥"}, {"ge", "≥"},
            {"neq", "≠"}, {"ne", "≠"}, {"approx", "≈"}, {"equiv", "≡"},
            {"to", "→"}, {"rightarrow", "→"}, {"leftarrow", "←"}, {"Rightarrow", "⇒"},
            {"Leftrightarrow", "⇔"}, {"leftrightarrow", "↔"},
            {"sum", "∑"}, {"prod", "∏"}, {"int", "∫"}, {"oint", "∮"},
            {"forall", "∀"}, {"exists", "∃"}, {"in", "∈"}, {"notin", "∉"},
            {"subset", "⊂"}, {"subseteq", "⊆"}, {"cup", "∪"}, {"cap", "∩"},
            {"quad", "  "}, {"qquad", "    "}, {",", " "}, {";", " "}, {"|", "‖"},
            {"{", "{"}, {"}", "}"}
        };
        auto it = syms.find(cmd);
        return it != syms.end() ? it->second : "";
    }

    static std::string map_mathbb(const std::string& raw) {
        std::string out;
        for (char c : raw) {
            switch (c) {
                case 'R': out += "ℝ"; break;
                case 'N': out += "ℕ"; break;
                case 'Z': out += "ℤ"; break;
                case 'Q': out += "ℚ"; break;
                case 'C': out += "ℂ"; break;
                case 'E': out += "𝔼"; break;
                case 'P': out += "ℙ"; break;
                default: out.push_back(c); break;
            }
        }
        return make_mr(out, true);
    }

    std::string read_braced_raw() {
        skip_ws();
        if (pos_ >= s_.size()) return "";
        if (s_[pos_] != '{') return std::string(1, s_[pos_++]);
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

    std::string parse_group_content() {
        skip_ws();
        if (pos_ < s_.size() && s_[pos_] == '{') {
            ++pos_;
            return parse_sequence('}');
        }
        return parse_atom();
    }

    std::string convert_matrix_env(const std::string& env, const std::string& body) {
        std::vector<std::vector<std::string>> rows;
        std::vector<std::string> cur_row;
        std::string cell;
        for (size_t i = 0; i < body.size(); ++i) {
            if (body[i] == '\\' && i + 1 < body.size() && body[i + 1] == '\\') {
                cur_row.push_back(trim_ws(cell));
                cell.clear();
                rows.push_back(std::move(cur_row));
                cur_row.clear();
                ++i;
            } else if (body[i] == '&') {
                cur_row.push_back(trim_ws(cell));
                cell.clear();
            } else {
                cell.push_back(body[i]);
            }
        }
        if (!trim_ws(cell).empty() || !cur_row.empty()) {
            cur_row.push_back(trim_ws(cell));
            rows.push_back(std::move(cur_row));
        }

        size_t max_cols = 1;
        for (const auto& r : rows) max_cols = std::max(max_cols, r.size());

        std::ostringstream mat;
        mat << "<m:m><m:mPr><m:mcs><m:mc><m:mcPr><m:count m:val=\"" << max_cols
            << "\"/><m:mcJc m:val=\"center\"/></m:mcPr></m:mc></m:mcs></m:mPr>";
        for (const auto& r : rows) {
            mat << "<m:mr>";
            for (size_t c = 0; c < max_cols; ++c) {
                std::string cell_tex = (c < r.size()) ? r[c] : "";
                TexToOmml sub(cell_tex);
                mat << "<m:e>" << sub.parse_sequence(0) << "</m:e>";
            }
            mat << "</m:mr>";
        }
        mat << "</m:m>";

        std::string beg, end;
        if (env == "pmatrix") { beg = "("; end = ")"; }
        else if (env == "bmatrix") { beg = "["; end = "]"; }
        else if (env == "vmatrix") { beg = "|"; end = "|"; }
        else if (env == "Bmatrix") { beg = "{"; end = "}"; }
        else if (env == "cases") { beg = "{"; end = ""; }
        else { return mat.str(); }

        return "<m:d><m:dPr><m:begChr m:val=\"" + xml_escape(beg) + "\"/><m:endChr m:val=\"" +
               xml_escape(end) + "\"/></m:dPr><m:e>" + mat.str() + "</m:e></m:d>";
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
        // Check for large nary operators (\sum, \prod, \int, \oint)
        size_t save_pos = pos_;
        skip_ws();
        std::string nary_chr;
        if (pos_ < s_.size() && s_[pos_] == '\\') {
            size_t j = pos_ + 1;
            while (j < s_.size() && std::isalpha(static_cast<unsigned char>(s_[j]))) ++j;
            std::string cmd = s_.substr(pos_ + 1, j - pos_ - 1);
            if (cmd == "sum") nary_chr = "∑";
            else if (cmd == "prod") nary_chr = "∏";
            else if (cmd == "int") nary_chr = "∫";
            else if (cmd == "oint") nary_chr = "∮";
            if (!nary_chr.empty()) {
                pos_ = j;
                bool has_sub = false, has_sup = false;
                std::string sub, sup;
                for (int pass = 0; pass < 2; ++pass) {
                    skip_ws();
                    if (pos_ < s_.size() && s_[pos_] == '_' && !has_sub) {
                        ++pos_;
                        sub = parse_group_content();
                        has_sub = true;
                    } else if (pos_ < s_.size() && s_[pos_] == '^' && !has_sup) {
                        ++pos_;
                        sup = parse_group_content();
                        has_sup = true;
                    } else break;
                }
                if (has_sub || has_sup) {
                    // Use sSubSup / sSub / sSup around the operator symbol (cleanly rendered in both WPS & Word!)
                    std::string base = make_mr(nary_chr, true);
                    if (has_sub && has_sup) return "<m:sSubSup><m:e>" + base + "</m:e><m:sub>" + sub + "</m:sub><m:sup>" + sup + "</m:sup></m:sSubSup>";
                    if (has_sub) return "<m:sSub><m:e>" + base + "</m:e><m:sub>" + sub + "</m:sub></m:sSub>";
                    return "<m:sSup><m:e>" + base + "</m:e><m:sup>" + sup + "</m:sup></m:sSup>";
                }
                return make_mr(nary_chr, true);
            }
        }
        pos_ = save_pos;

        std::string base = parse_atom();
        skip_ws();

        bool has_sub = false, has_sup = false;
        std::string sub, sup;

        for (int pass = 0; pass < 2; ++pass) {
            skip_ws();
            if (pos_ < s_.size() && s_[pos_] == '_' && !has_sub) {
                ++pos_;
                sub = parse_group_content();
                has_sub = true;
            } else if (pos_ < s_.size() && s_[pos_] == '^' && !has_sup) {
                ++pos_;
                sup = parse_group_content();
                has_sup = true;
            } else {
                break;
            }
        }

        if (has_sub && has_sup) {
            return "<m:sSubSup><m:e>" + base + "</m:e><m:sub>" + sub + "</m:sub><m:sup>" + sup + "</m:sup></m:sSubSup>";
        }
        if (has_sub) {
            return "<m:sSub><m:e>" + base + "</m:e><m:sub>" + sub + "</m:sub></m:sSub>";
        }
        if (has_sup) {
            return "<m:sSup><m:e>" + base + "</m:e><m:sup>" + sup + "</m:sup></m:sSup>";
        }
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
                std::string num = parse_group_content();
                std::string den = parse_group_content();
                return "<m:f><m:num>" + num + "</m:num><m:den>" + den + "</m:den></m:f>";
            }
            if (cmd == "sqrt") {
                skip_ws();
                std::string deg;
                if (pos_ < s_.size() && s_[pos_] == '[') {
                    ++pos_;
                    deg = parse_sequence(']');
                }
                std::string rad_e = parse_group_content();
                if (!deg.empty()) {
                    return "<m:rad><m:deg>" + deg + "</m:deg><m:e>" + rad_e + "</m:e></m:rad>";
                }
                return "<m:rad><m:radPr><m:degHide m:val=\"1\"/></m:radPr><m:deg/><m:e>" + rad_e + "</m:e></m:rad>";
            }
            if (cmd == "begin") {
                std::string env = trim_ws(read_braced_raw());
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
                return convert_matrix_env(env, body);
            }
            if (cmd == "mathbb") {
                return map_mathbb(read_braced_raw());
            }
            if (cmd == "overline") {
                return "<m:bar><m:barPr><m:pos m:val=\"top\"/></m:barPr><m:e>" + parse_group_content() + "</m:e></m:bar>";
            }
            if (cmd == "hat" || cmd == "bar" || cmd == "vec" || cmd == "tilde" || cmd == "dot") {
                std::string chr = "^";
                if (cmd == "bar") chr = "¯";
                else if (cmd == "vec") chr = "→";
                else if (cmd == "tilde") chr = "~";
                else if (cmd == "dot") chr = "˙";
                return "<m:acc><m:accPr><m:chr m:val=\"" + xml_escape(chr) + "\"/></m:accPr><m:e>" + parse_group_content() + "</m:e></m:acc>";
            }
            if (cmd == "text") {
                return make_mr(read_braced_raw(), true);
            }
            if (cmd == "mathbf" || cmd == "mathcal" || cmd == "mathrm" || cmd == "operatorname") {
                return parse_group_content();
            }
            if (cmd == "sin" || cmd == "cos" || cmd == "tan" || cmd == "log" || cmd == "ln" ||
                cmd == "exp" || cmd == "lim" || cmd == "max" || cmd == "min" || cmd == "sup" ||
                cmd == "inf" || cmd == "arg" || cmd == "det" || cmd == "dim") {
                return make_mr(cmd, true);
            }
            if (cmd == "left" || cmd == "right") {
                skip_ws();
                if (pos_ < s_.size()) {
                    char delim = s_[pos_++];
                    if (delim == '\\') {
                        if (pos_ < s_.size()) {
                            char d2 = s_[pos_++];
                            return make_mr(d2 == '|' ? "‖" : std::string(1, d2), true);
                        }
                    }
                    if (delim == '.') return "";
                    return make_mr(std::string(1, delim), true);
                }
                return "";
            }
            std::string sym = map_symbol(cmd);
            if (!sym.empty()) {
                return make_mr(sym, true);
            }
            return make_mr(cmd, true);
        }

        // Plain identifier/number/operator (UTF-8 aware)
        unsigned char uc = static_cast<unsigned char>(c);
        size_t len = 1;
        if ((uc & 0xE0) == 0xC0) len = 2;
        else if ((uc & 0xF0) == 0xE0) len = 3;
        else if ((uc & 0xF8) == 0xF0) len = 4;
        std::string ch = s_.substr(pos_, std::min(len, s_.size() - pos_));
        pos_ += ch.size();
        bool plain = std::isdigit(uc) || ch == "=" || ch == "+" || ch == "-" || ch == "(" || ch == ")" ||
                     ch == "[" || ch == "]" || ch == "," || ch == "." || ch == "<" || ch == ">" || ch == "|";
        return make_mr(ch, plain);
    }
};

// ============================================================================
// OOXML Package XML Builders (WPS & MS Word compatible)
// ============================================================================
std::string build_content_types_xml(const std::set<std::string>& media_exts) {
    std::ostringstream oss;
    oss << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">
  <Default Extension="rels" ContentType="application/vnd.openxmlformats-package.relationships+xml"/>
  <Default Extension="xml" ContentType="application/xml"/>
)";
    for (const auto& ext : media_exts) {
        if (ext == "png") oss << "  <Default Extension=\"png\" ContentType=\"image/png\"/>\n";
        else if (ext == "jpg" || ext == "jpeg") oss << "  <Default Extension=\"" << ext << "\" ContentType=\"image/jpeg\"/>\n";
        else if (ext == "gif") oss << "  <Default Extension=\"gif\" ContentType=\"image/gif\"/>\n";
        else if (ext == "bmp") oss << "  <Default Extension=\"bmp\" ContentType=\"image/bmp\"/>\n";
        else if (ext == "webp") oss << "  <Default Extension=\"webp\" ContentType=\"image/webp\"/>\n";
    }
    oss << R"(  <Override PartName="/word/document.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.document.main+xml"/>
  <Override PartName="/word/styles.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.styles+xml"/>
  <Override PartName="/word/numbering.xml" ContentType="application/vnd.openxmlformats-officedocument.wordprocessingml.numbering+xml"/>
</Types>
)";
    return oss.str();
}

std::string build_root_rels_xml() {
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="word/document.xml"/>
</Relationships>
)";
}

struct DocRel {
    std::string id;
    std::string type;
    std::string target;
    bool external = false;
};

std::string build_doc_rels_xml(const std::vector<DocRel>& rels) {
    std::ostringstream oss;
    oss << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">
  <Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/styles" Target="styles.xml"/>
  <Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/numbering" Target="numbering.xml"/>
)";
    for (const auto& r : rels) {
        oss << "  <Relationship Id=\"" << xml_escape(r.id) << "\" Type=\"" << xml_escape(r.type)
            << "\" Target=\"" << xml_escape(r.target) << "\""
            << (r.external ? " TargetMode=\"External\"" : "") << "/>\n";
    }
    oss << "</Relationships>\n";
    return oss.str();
}

std::string build_styles_xml() {
    return R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:styles xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:docDefaults>
    <w:rPrDefault>
      <w:rPr>
        <w:rFonts w:ascii="Times New Roman" w:eastAsia="宋体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/>
        <w:sz w:val="21"/>
        <w:szCs w:val="21"/>
      </w:rPr>
    </w:rPrDefault>
    <w:pPrDefault>
      <w:pPr>
        <w:spacing w:after="120" w:line="300" w:lineRule="auto"/>
      </w:pPr>
    </w:pPrDefault>
  </w:docDefaults>
  <w:style w:type="paragraph" w:default="1" w:styleId="Normal">
    <w:name w:val="Normal"/>
    <w:qFormat/>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading1">
    <w:name w:val="heading 1"/>
    <w:basedOn w:val="Normal"/>
    <w:next w:val="Normal"/>
    <w:qFormat/>
    <w:pPr><w:spacing w:before="240" w:after="120"/><w:outlineLvl w:val="0"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="黑体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/><w:b/><w:color w:val="111827"/><w:sz w:val="44"/><w:szCs w:val="44"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading2">
    <w:name w:val="heading 2"/>
    <w:basedOn w:val="Normal"/>
    <w:next w:val="Normal"/>
    <w:qFormat/>
    <w:pPr><w:spacing w:before="200" w:after="100"/><w:outlineLvl w:val="1"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="黑体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/><w:b/><w:color w:val="1F2937"/><w:sz w:val="32"/><w:szCs w:val="32"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading3">
    <w:name w:val="heading 3"/>
    <w:basedOn w:val="Normal"/>
    <w:next w:val="Normal"/>
    <w:qFormat/>
    <w:pPr><w:spacing w:before="160" w:after="80"/><w:outlineLvl w:val="2"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="黑体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/><w:b/><w:color w:val="374151"/><w:sz w:val="28"/><w:szCs w:val="28"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading4">
    <w:name w:val="heading 4"/>
    <w:basedOn w:val="Normal"/>
    <w:next w:val="Normal"/>
    <w:qFormat/>
    <w:pPr><w:spacing w:before="120" w:after="60"/><w:outlineLvl w:val="3"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="黑体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/><w:b/><w:sz w:val="24"/><w:szCs w:val="24"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading5">
    <w:name w:val="heading 5"/>
    <w:basedOn w:val="Normal"/>
    <w:next w:val="Normal"/>
    <w:qFormat/>
    <w:pPr><w:spacing w:before="100" w:after="40"/><w:outlineLvl w:val="4"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="黑体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/><w:b/><w:sz w:val="22"/><w:szCs w:val="22"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Heading6">
    <w:name w:val="heading 6"/>
    <w:basedOn w:val="Normal"/>
    <w:next w:val="Normal"/>
    <w:qFormat/>
    <w:pPr><w:spacing w:before="80" w:after="40"/><w:outlineLvl w:val="5"/></w:pPr>
    <w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="黑体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/><w:b/><w:sz w:val="21"/><w:szCs w:val="21"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="Quote">
    <w:name w:val="Quote"/>
    <w:basedOn w:val="Normal"/>
    <w:pPr>
      <w:ind w:left="480"/>
      <w:pBdr><w:left w:val="single" w:sz="18" w:space="12" w:color="3B82F6"/></w:pBdr>
      <w:shd w:val="clear" w:color="auto" w:fill="F8FAFC"/>
    </w:pPr>
    <w:rPr><w:rFonts w:ascii="Times New Roman" w:eastAsia="楷体" w:hAnsi="Times New Roman" w:cs="Times New Roman"/><w:color w:val="475569"/></w:rPr>
  </w:style>
  <w:style w:type="paragraph" w:styleId="CodeBlock">
    <w:name w:val="CodeBlock"/>
    <w:basedOn w:val="Normal"/>
    <w:pPr>
      <w:spacing w:before="80" w:after="100" w:line="260" w:lineRule="auto"/>
      <w:ind w:left="200" w:right="200"/>
      <w:pBdr>
        <w:top w:val="single" w:sz="6" w:space="6" w:color="CBD5E1"/>
        <w:left w:val="single" w:sz="6" w:space="8" w:color="CBD5E1"/>
        <w:bottom w:val="single" w:sz="6" w:space="6" w:color="CBD5E1"/>
        <w:right w:val="single" w:sz="6" w:space="8" w:color="CBD5E1"/>
      </w:pBdr>
      <w:shd w:val="clear" w:color="auto" w:fill="F3F4F6"/>
    </w:pPr>
    <w:rPr><w:rFonts w:ascii="Consolas" w:eastAsia="宋体" w:hAnsi="Consolas" w:cs="Consolas"/><w:sz w:val="19"/><w:szCs w:val="19"/></w:rPr>
  </w:style>
</w:styles>
)";
}

std::string build_numbering_xml() {
    std::ostringstream oss;
    oss << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:numbering xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main">
  <w:abstractNum w:abstractNumId="1">
    <w:multiLevelType w:val="hybridMultilevel"/>
)";
    const char* bullets[] = {"•", "◦", "▪", "•", "◦", "▪"};
    for (int lvl = 0; lvl < 6; ++lvl) {
        int left_tw = 420 * (lvl + 1);
        oss << "    <w:lvl w:ilvl=\"" << lvl << "\">\n"
            << "      <w:start w:val=\"1\"/><w:numFmt w:val=\"bullet\"/>\n"
            << "      <w:lvlText w:val=\"" << bullets[lvl] << "\"/><w:lvlJc w:val=\"left\"/>\n"
            << "      <w:pPr><w:ind w:left=\"" << left_tw << "\" w:hanging=\"260\"/></w:pPr>\n"
            << "      <w:rPr><w:rFonts w:ascii=\"Times New Roman\" w:eastAsia=\"宋体\" w:hAnsi=\"Times New Roman\" w:hint=\"default\"/></w:rPr>\n"
            << "    </w:lvl>\n";
    }
    oss << R"(  </w:abstractNum>
  <w:abstractNum w:abstractNumId="2">
    <w:multiLevelType w:val="hybridMultilevel"/>
)";
    const char* num_fmts[] = {"decimal", "lowerLetter", "lowerRoman", "decimal", "lowerLetter", "lowerRoman"};
    for (int lvl = 0; lvl < 6; ++lvl) {
        int left_tw = 420 * (lvl + 1);
        oss << "    <w:lvl w:ilvl=\"" << lvl << "\">\n"
            << "      <w:start w:val=\"1\"/><w:numFmt w:val=\"" << num_fmts[lvl] << "\"/>\n"
            << "      <w:lvlText w:val=\"%" << (lvl + 1) << ".\"/><w:lvlJc w:val=\"left\"/>\n"
            << "      <w:pPr><w:ind w:left=\"" << left_tw << "\" w:hanging=\"280\"/></w:pPr>\n"
            << "    </w:lvl>\n";
    }
    oss << R"(  </w:abstractNum>
  <w:num w:numId="1"><w:abstractNumId w:val="1"/></w:num>
  <w:num w:numId="2"><w:abstractNumId w:val="2"/></w:num>
</w:numbering>
)";
    return oss.str();
}

void write_font_rpr(std::ostringstream& oss, const std::string& font_family) {
    if (font_family.empty()) return;
    std::string low = to_lower(font_family);
    if (low.find("consolas") != std::string::npos || low.find("courier") != std::string::npos ||
        low.find("jetbrains") != std::string::npos || low.find("mono") != std::string::npos) {
        oss << "<w:rFonts w:ascii=\"" << xml_escape(font_family) << "\" w:eastAsia=\"宋体\" w:hAnsi=\""
            << xml_escape(font_family) << "\" w:cs=\"" << xml_escape(font_family) << "\"/>";
    } else if (low.find("times") != std::string::npos || low.find("calibri") != std::string::npos ||
               low.find("cambria") != std::string::npos || low.find("arial") != std::string::npos) {
        oss << "<w:rFonts w:ascii=\"" << xml_escape(font_family) << "\" w:eastAsia=\"宋体\" w:hAnsi=\""
            << xml_escape(font_family) << "\" w:cs=\"" << xml_escape(font_family) << "\"/>";
    } else {
        // Chinese font chosen (宋体 / 黑体 / 楷体 / 微软雅黑): keep Times New Roman for ASCII
        oss << "<w:rFonts w:ascii=\"Times New Roman\" w:eastAsia=\"" << xml_escape(font_family)
            << "\" w:hAnsi=\"Times New Roman\" w:cs=\"Times New Roman\"/>";
    }
}

void write_run_rpr(std::ostringstream& oss, const InlineSpan& sp, const BlockProps& blk_props) {
    std::string eff_font = !sp.font_family.empty() ? sp.font_family : blk_props.font_family;
    double eff_size = sp.font_size_pt > 0.0 ? sp.font_size_pt : blk_props.font_size_pt;
    std::string eff_color = normalize_hex(sp.color);
    std::string eff_hl = normalize_hex(sp.highlight);

    bool need_rpr = sp.bold || sp.italic || sp.underline || sp.strike || sp.sup || sp.sub ||
                    sp.code || !eff_font.empty() || eff_size > 0.0 || !eff_color.empty() || !eff_hl.empty();
    if (!need_rpr) return;

    oss << "<w:rPr>";
    if (sp.code && eff_font.empty()) {
        oss << "<w:rFonts w:ascii=\"Consolas\" w:eastAsia=\"宋体\" w:hAnsi=\"Consolas\"/>";
    } else if (!eff_font.empty()) {
        write_font_rpr(oss, eff_font);
    }
    if (sp.bold) oss << "<w:b/>";
    if (sp.italic) oss << "<w:i/>";
    if (sp.underline) oss << "<w:u w:val=\"single\"/>";
    if (sp.strike) oss << "<w:strike/>";
    if (sp.sup) oss << "<w:vertAlign w:val=\"superscript\"/>";
    else if (sp.sub) oss << "<w:vertAlign w:val=\"subscript\"/>";
    if (!eff_color.empty()) oss << "<w:color w:val=\"" << eff_color << "\"/>";
    if (eff_size > 0.0) {
        int half_pts = static_cast<int>(std::lround(eff_size * 2.0));
        oss << "<w:sz w:val=\"" << half_pts << "\"/><w:szCs w:val=\"" << half_pts << "\"/>";
    }
    if (!eff_hl.empty()) {
        oss << "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"" << eff_hl << "\"/>";
    } else if (sp.code) {
        oss << "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"F1F5F9\"/>";
    }
    oss << "</w:rPr>";
}

struct ExportMediaState {
    ZipArchive* zip = nullptr;
    std::vector<DocRel> rels;
    std::set<std::string> media_exts;
    int next_rel_id = 3; // rId1=styles.xml, rId2=numbering.xml
    int next_img_id = 1;
    std::string base_dir;
    const std::map<std::string, std::string>* diagram_pngs = nullptr;
};

std::string build_drawing_ooxml(const std::string& rid,
                                int docpr_id,
                                const std::string& name,
                                const std::string& descr,
                                int px_w,
                                int px_h) {
    int64_t cx = static_cast<int64_t>(px_w) * 9525LL;
    int64_t cy = static_cast<int64_t>(px_h) * 9525LL;
    const int64_t max_cx = 5400000LL; // ~15cm fits A4 page margins
    if (cx > max_cx && cx > 0) {
        cy = cy * max_cx / cx;
        cx = max_cx;
    }
    if (cx <= 0) { cx = 3600000LL; cy = 2000000LL; }

    std::ostringstream oss;
    oss << "<w:r><w:drawing><wp:inline distT=\"0\" distB=\"0\" distL=\"0\" distR=\"0\">"
        << "<wp:extent cx=\"" << cx << "\" cy=\"" << cy << "\"/>"
        << "<wp:docPr id=\"" << docpr_id << "\" name=\"" << xml_escape(name)
        << "\" descr=\"" << xml_escape(descr) << "\"/>"
        << "<wp:cNvGraphicFramePr><a:graphicFrameLocks noChangeAspect=\"1\"/></wp:cNvGraphicFramePr>"
        << "<a:graphic><a:graphicData uri=\"http://schemas.openxmlformats.org/drawingml/2006/picture\">"
        << "<pic:pic>"
        << "<pic:nvPicPr><pic:cNvPr id=\"" << docpr_id << "\" name=\"" << xml_escape(name)
        << "\"/><pic:cNvPicPr/></pic:nvPicPr>"
        << "<pic:blipFill><a:blip r:embed=\"" << rid << "\"/><a:stretch><a:fillRect/></a:stretch></pic:blipFill>"
        << "<pic:spPr><a:xfrm><a:off x=\"0\" y=\"0\"/><a:ext cx=\"" << cx << "\" cy=\"" << cy << "\"/></a:xfrm>"
        << "<a:prstGeom prst=\"rect\"><a:avLst/></a:prstGeom></pic:spPr>"
        << "</pic:pic></a:graphicData></a:graphic></wp:inline></w:drawing></w:r>";
    return oss.str();
}

bool load_image_source(const std::string& url,
                       const std::string& base_dir,
                       std::string& out_bytes,
                       std::string& out_ext) {
    if (url.rfind("data:image/", 0) == 0) {
        size_t semi = url.find(';');
        size_t comma = url.find(',');
        if (semi == std::string::npos || comma == std::string::npos || semi > comma) return false;
        out_ext = to_lower(url.substr(11, semi - 11));
        if (out_ext == "jpeg") out_ext = "jpg";
        if (out_ext == "svg+xml") return false;
        out_bytes = base64_decode(url.substr(comma + 1));
        return !out_bytes.empty();
    }
    fs::path p = utf8_to_path(url);
    std::error_code ec;
    if (!p.is_absolute() && !base_dir.empty()) {
        p = utf8_to_path(base_dir) / p;
    }
    if (!fs::exists(p, ec)) return false;
    std::ifstream ifs(p, std::ios::binary);
    if (!ifs) return false;
    std::ostringstream ss;
    ss << ifs.rdbuf();
    out_bytes = ss.str();
    out_ext = to_lower(path_to_utf8(p.extension()));
    if (!out_ext.empty() && out_ext[0] == '.') out_ext = out_ext.substr(1);
    if (out_ext.empty()) out_ext = "png";
    return !out_bytes.empty();
}

void write_inlines_ooxml(std::ostringstream& oss,
                         const std::vector<InlineSpan>& spans,
                         const BlockProps& blk_props,
                         ExportMediaState& ms) {
    for (const auto& sp : spans) {
        if (sp.kind == InlineSpan::Kind::LineBreak) {
            oss << "<w:r><w:br/></w:r>";
            continue;
        }
        if (sp.kind == InlineSpan::Kind::InlineMath) {
            oss << latex_to_omml(sp.text);
            continue;
        }
        if (sp.kind == InlineSpan::Kind::Wikilink) {
            oss << "<w:r><w:rPr><w:b/><w:color w:val=\"1D4ED8\"/></w:rPr>"
                << "<w:t xml:space=\"preserve\">[[" << xml_escape(sp.text) << "]]</w:t></w:r>";
            continue;
        }
        if (sp.kind == InlineSpan::Kind::Link) {
            std::string rid = "rId" + std::to_string(ms.next_rel_id++);
            ms.rels.push_back({rid, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/hyperlink", sp.url, true});
            oss << "<w:hyperlink r:id=\"" << rid << "\">"
                << "<w:r><w:rPr><w:color w:val=\"2563EB\"/><w:u w:val=\"single\"/></w:rPr>"
                << "<w:t xml:space=\"preserve\">" << xml_escape(sp.text) << "</w:t></w:r>"
                << "</w:hyperlink>";
            continue;
        }
        if (sp.kind == InlineSpan::Kind::Image) {
            std::string img_bytes, img_ext;
            if (load_image_source(sp.url, ms.base_dir, img_bytes, img_ext)) {
                int img_id = ms.next_img_id++;
                std::string media_rel = "media/image" + std::to_string(img_id) + "." + img_ext;
                ms.zip->set_entry("word/" + media_rel, img_bytes);
                ms.media_exts.insert(img_ext);
                std::string rid = "rId" + std::to_string(ms.next_rel_id++);
                ms.rels.push_back({rid, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image", media_rel, false});
                int pw = 480, ph = 280;
                read_image_dimensions(img_bytes, img_ext, pw, ph);
                oss << build_drawing_ooxml(rid, img_id, "Image" + std::to_string(img_id), sp.text, pw, ph);
            } else {
                oss << "<w:r><w:t xml:space=\"preserve\">![";
                oss << xml_escape(sp.text) << "](" << xml_escape(sp.url) << ")</w:t></w:r>";
            }
            continue;
        }

        // Standard Text run with full composable rPr
        oss << "<w:r>";
        write_run_rpr(oss, sp, blk_props);
        oss << "<w:t xml:space=\"preserve\">" << xml_escape(sp.text) << "</w:t></w:r>";
    }
}

void write_paragraph_ppr_extras(std::ostringstream& oss, const BlockProps& p) {
    bool has_custom_line = (!p.line_rule.empty() && (p.line_rule == "atLeast" || p.line_rule == "exact") && p.line_pt > 0.0) ||
                           (p.line_spacing > 0.0);
    if (p.space_before_pt >= 0.0 || p.space_after_pt >= 0.0 || has_custom_line) {
        oss << "<w:spacing";
        if (p.space_before_pt >= 0.0) oss << " w:before=\"" << static_cast<int>(std::lround(p.space_before_pt * 20.0)) << "\"";
        if (p.space_after_pt >= 0.0) oss << " w:after=\"" << static_cast<int>(std::lround(p.space_after_pt * 20.0)) << "\"";
        if (!p.line_rule.empty() && (p.line_rule == "atLeast" || p.line_rule == "exact") && p.line_pt > 0.0) {
            oss << " w:line=\"" << static_cast<int>(std::lround(p.line_pt * 20.0)) << "\" w:lineRule=\"" << p.line_rule << "\"";
        } else if (p.line_spacing > 0.0) {
            oss << " w:line=\"" << static_cast<int>(std::lround(p.line_spacing * 240.0)) << "\" w:lineRule=\"auto\"";
        }
        oss << "/>";
    }
    if (p.first_line_indent_chars > 0.0) {
        int chars_hundredths = static_cast<int>(std::lround(p.first_line_indent_chars * 100.0));
        int twips = static_cast<int>(std::lround(p.first_line_indent_chars * 210.0));
        oss << "<w:ind w:firstLine=\"" << twips << "\" w:firstLineChars=\"" << chars_hundredths << "\"/>";
    }
    if (!p.align.empty()) {
        std::string jc = (p.align == "justify") ? "both" : p.align;
        if (jc == "left" || jc == "center" || jc == "right" || jc == "both") {
            oss << "<w:jc w:val=\"" << jc << "\"/>";
        }
    }
    if (p.has_border) {
        oss << "<w:pBdr>"
            << "<w:top w:val=\"single\" w:sz=\"6\" w:space=\"4\" w:color=\"CBD5E1\"/>"
            << "<w:left w:val=\"single\" w:sz=\"6\" w:space=\"6\" w:color=\"CBD5E1\"/>"
            << "<w:bottom w:val=\"single\" w:sz=\"6\" w:space=\"4\" w:color=\"CBD5E1\"/>"
            << "<w:right w:val=\"single\" w:sz=\"6\" w:space=\"6\" w:color=\"CBD5E1\"/>"
            << "</w:pBdr>";
    }
    std::string bg = normalize_hex(p.shading);
    if (!bg.empty()) {
        oss << "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"" << bg << "\"/>";
    }
}

// ============================================================================
// Word OMML -> LaTeX extractor
// ============================================================================
std::string reverse_symbol_unicode(const std::string& s) {
    static const std::pair<const char*, const char*> reps[] = {
        {"α", "\\alpha"}, {"β", "\\beta"}, {"γ", "\\gamma"}, {"δ", "\\delta"},
        {"ε", "\\varepsilon"}, {"ϵ", "\\epsilon"}, {"θ", "\\theta"}, {"λ", "\\lambda"},
        {"μ", "\\mu"}, {"π", "\\pi"}, {"ρ", "\\rho"}, {"σ", "\\sigma"},
        {"φ", "\\phi"}, {"ω", "\\omega"}, {"Δ", "\\Delta"}, {"Σ", "\\Sigma"},
        {"Ω", "\\Omega"}, {"∞", "\\infty"}, {"∂", "\\partial"}, {"∇", "\\nabla"},
        {"·", "\\cdot"}, {"×", "\\times"}, {"±", "\\pm"}, {"≤", "\\leq"},
        {"≥", "\\geq"}, {"≠", "\\neq"}, {"≈", "\\approx"}, {"→", "\\to"},
        {"⇒", "\\Rightarrow"}, {"⇔", "\\Leftrightarrow"}, {"∑", "\\sum"}, {"∏", "\\prod"}, {"∫", "\\int"},
        {"ℝ", "\\mathbb{R}"}, {"ℕ", "\\mathbb{N}"}, {"ℤ", "\\mathbb{Z}"},
        {"ℚ", "\\mathbb{Q}"}, {"ℂ", "\\mathbb{C}"}, {"𝔼", "\\mathbb{E}"}, {"ℙ", "\\mathbb{P}"}
    };
    std::string out = s;
    for (const auto& r : reps) {
        size_t pos = 0;
        std::string from(r.first);
        std::string to(std::string(r.second) + " ");
        while ((pos = out.find(from, pos)) != std::string::npos) {
            out.replace(pos, from.size(), to);
            pos += to.size();
        }
    }
    return out;
}

std::string extract_omml_recursive(const XmlNode* node) {
    if (!node) return "";
    if (node->is_text) return node->text;
    std::string ln = node->local_name();

    if (ln == "t") {
        return reverse_symbol_unicode(node->inner_text());
    }
    if (ln == "f") {
        auto num = node->first_child("num");
        auto den = node->first_child("den");
        return "\\frac{" + trim_ws(extract_omml_recursive(num.get())) + "}{" + trim_ws(extract_omml_recursive(den.get())) + "}";
    }
    if (ln == "rad") {
        auto deg = node->first_child("deg");
        auto e = node->first_child("e");
        std::string deg_s = trim_ws(extract_omml_recursive(deg.get()));
        std::string e_s = trim_ws(extract_omml_recursive(e.get()));
        if (!deg_s.empty()) return "\\sqrt[" + deg_s + "]{" + e_s + "}";
        return "\\sqrt{" + e_s + "}";
    }
    if (ln == "sSup") {
        auto e = node->first_child("e");
        auto sup = node->first_child("sup");
        return extract_omml_recursive(e.get()) + "^{" + trim_ws(extract_omml_recursive(sup.get())) + "}";
    }
    if (ln == "sSub") {
        auto e = node->first_child("e");
        auto sub = node->first_child("sub");
        return extract_omml_recursive(e.get()) + "_{" + trim_ws(extract_omml_recursive(sub.get())) + "}";
    }
    if (ln == "sSubSup") {
        auto e = node->first_child("e");
        auto sub = node->first_child("sub");
        auto sup = node->first_child("sup");
        return extract_omml_recursive(e.get()) + "_{" + trim_ws(extract_omml_recursive(sub.get())) + "}^{" + trim_ws(extract_omml_recursive(sup.get())) + "}";
    }
    if (ln == "nary") {
        std::string chr = "\\sum";
        if (auto np = node->first_child("naryPr")) {
            if (auto c = np->first_child("chr")) {
                std::string cv = c->attr("val");
                if (cv == "∏") chr = "\\prod";
                else if (cv == "∫") chr = "\\int";
                else if (cv == "∮") chr = "\\oint";
            }
        }
        std::string sub_s = trim_ws(extract_omml_recursive(node->first_child("sub").get()));
        std::string sup_s = trim_ws(extract_omml_recursive(node->first_child("sup").get()));
        std::string e_s = trim_ws(extract_omml_recursive(node->first_child("e").get()));
        std::string out = chr;
        if (!sub_s.empty()) out += "_{" + sub_s + "}";
        if (!sup_s.empty()) out += "^{" + sup_s + "}";
        if (!e_s.empty()) out += " " + e_s;
        return out;
    }
    if (ln == "acc") {
        std::string chr = "^";
        if (auto ap = node->first_child("accPr")) {
            if (auto c = ap->first_child("chr")) chr = c->attr("val", "^");
        }
        std::string cmd = "\\hat";
        if (chr == "→") cmd = "\\vec";
        else if (chr == "¯") cmd = "\\bar";
        else if (chr == "~") cmd = "\\tilde";
        else if (chr == "˙") cmd = "\\dot";
        return cmd + "{" + trim_ws(extract_omml_recursive(node->first_child("e").get())) + "}";
    }
    if (ln == "bar") {
        return "\\overline{" + trim_ws(extract_omml_recursive(node->first_child("e").get())) + "}";
    }
    if (ln == "m") {
        std::string out;
        auto mrs = node->children_named("mr");
        for (size_t r = 0; r < mrs.size(); ++r) {
            if (r > 0) out += " \\\\ ";
            auto es = mrs[r]->children_named("e");
            for (size_t c = 0; c < es.size(); ++c) {
                if (c > 0) out += " & ";
                out += trim_ws(extract_omml_recursive(es[c].get()));
            }
        }
        return out;
    }
    if (ln == "d") {
        std::string beg = "(", end = ")";
        if (auto dp = node->first_child("dPr")) {
            if (auto b = dp->first_child("begChr")) beg = b->attr("val", "(");
            if (auto en = dp->first_child("endChr")) end = en->attr("val", ")");
        }
        auto e = node->first_child("e");
        if (e && e->first_child("m")) {
            std::string env = "pmatrix";
            if (beg == "[") env = "bmatrix";
            else if (beg == "|") env = "vmatrix";
            else if (beg == "{" && end.empty()) env = "cases";
            else if (beg == "{") env = "Bmatrix";
            return "\\begin{" + env + "}" + trim_ws(extract_omml_recursive(e.get())) + "\\end{" + env + "}";
        }
        return beg + trim_ws(extract_omml_recursive(e.get())) + end;
    }
    if (ln == "rPr" || ln == "radPr" || ln == "ctrlPr" || ln == "fPr" || ln == "mPr" ||
        ln == "dPr" || ln == "accPr" || ln == "barPr" || ln == "naryPr") {
        return "";
    }

    std::string out;
    for (const auto& c : node->children) {
        out += extract_omml_recursive(c.get());
    }
    return out;
}

int detect_heading_level(const XmlNode* p_node) {
    auto pPr = p_node->first_child("pPr");
    if (!pPr) return 0;
    auto pStyle = pPr->first_child("pStyle");
    if (pStyle) {
        std::string val = to_lower(pStyle->attr("val"));
        if (val == "title") return 1;
        for (int lvl = 1; lvl <= 6; ++lvl) {
            std::string num = std::to_string(lvl);
            if (val == "heading" + num || val == "heading " + num || val == num ||
                val == "a" + num || val.find("heading" + num) != std::string::npos) {
                return lvl;
            }
        }
    }
    auto outline = pPr->first_child("outlineLvl");
    if (outline) {
        try {
            int v = std::stoi(outline->attr("val"));
            if (v >= 0 && v <= 5) return v + 1;
        } catch (...) {}
    }
    return 0;
}

bool is_quote_paragraph(const XmlNode* p_node) {
    auto pPr = p_node->first_child("pPr");
    if (!pPr) return false;
    auto pStyle = pPr->first_child("pStyle");
    if (pStyle && to_lower(pStyle->attr("val")).find("quote") != std::string::npos) return true;
    return false;
}

bool is_codeblock_paragraph(const XmlNode* p_node, std::string& out_lang) {
    auto pPr = p_node->first_child("pPr");
    if (pPr) {
        auto pStyle = pPr->first_child("pStyle");
        if (pStyle && to_lower(pStyle->attr("val")) == "codeblock") {
            for (const auto& bm : p_node->children_named("bookmarkStart")) {
                std::string nm = bm->attr("name");
                if (nm.rfind("OMNI_CODE_", 0) == 0) out_lang = nm.substr(10);
            }
            return true;
        }
    }
    return false;
}

bool extract_list_info(const XmlNode* p_node,
                       const std::map<std::string, bool>& num_is_ordered,
                       bool& out_ordered,
                       int& out_ilvl) {
    auto pPr = p_node->first_child("pPr");
    if (!pPr) return false;
    auto numPr = pPr->first_child("numPr");
    if (!numPr) return false;
    out_ilvl = 0;
    if (auto il = numPr->first_child("ilvl")) {
        try { out_ilvl = std::max(0, std::stoi(il->attr("val", "0"))); } catch (...) {}
    }
    out_ordered = false;
    if (auto nid = numPr->first_child("numId")) {
        auto it = num_is_ordered.find(nid->attr("val"));
        if (it != num_is_ordered.end()) out_ordered = it->second;
    }
    return true;
}

std::string find_attr_recursive(const XmlNode* node, const std::string& elem_local, const std::string& attr_key) {
    if (!node || node->is_text) return "";
    if (node->local_name() == elem_local) {
        std::string v = node->attr(attr_key);
        if (!v.empty()) return v;
    }
    for (const auto& c : node->children) {
        std::string r = find_attr_recursive(c.get(), elem_local, attr_key);
        if (!r.empty()) return r;
    }
    return "";
}

std::string extract_run_markdown(const XmlNode* r_node,
                                 const ZipArchive& zip,
                                 const std::map<std::string, std::string>& rels,
                                 bool& out_has_figma,
                                 std::string& out_figma_json,
                                 bool is_heading = false) {
    // Check for drawing / embedded image / figma diagram first
    if (auto dr = r_node->first_child("drawing")) {
        std::string descr = find_attr_recursive(dr.get(), "docPr", "descr");
        if (descr.rfind("OMNI_FIGMA:", 0) == 0) {
            out_has_figma = true;
            out_figma_json = base64_decode(descr.substr(11));
            return "";
        }
        std::string embed_rid = find_attr_recursive(dr.get(), "blip", "embed");
        if (!embed_rid.empty()) {
            auto it = rels.find(embed_rid);
            if (it != rels.end()) {
                std::string target = it->second;
                if (target.rfind("word/", 0) != 0) target = "word/" + target;
                std::string img_bytes;
                if (zip.get_entry(target, img_bytes) && !img_bytes.empty()) {
                    std::string ext = to_lower(path_to_utf8(utf8_to_path(target).extension()));
                    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
                    if (ext == "jpg") ext = "jpeg";
                    if (ext.empty()) ext = "png";
                    std::string alt = descr.empty() ? "image" : descr;
                    return "![" + alt + "](data:image/" + ext + ";base64," + base64_encode(img_bytes) + ")";
                }
            }
        }
    }

    std::string text;
    for (const auto& c : r_node->children) {
        std::string ln = c->local_name();
        if (ln == "t" || ln == "delText") {
            text += c->inner_text();
        } else if (ln == "tab") {
            text += "    ";
        } else if (ln == "br") {
            if (to_lower(c->attr("type")) != "page") {
                text += "<br/>";
            }
        }
    }
    if (text.empty()) return "";

    bool bold = false, italic = false, underline = false, strike = false, is_code = false;
    bool sup = false, sub = false;
    std::string color_hex, hl_hex, custom_font;
    double size_pt = 0.0;

    if (auto rPr = r_node->first_child("rPr")) {
        auto b = rPr->first_child("b");
        if (b && b->attr("val", "1") != "0" && b->attr("val", "true") != "false") bold = true;
        auto i = rPr->first_child("i");
        if (i && i->attr("val", "1") != "0" && i->attr("val", "true") != "false") italic = true;
        auto u = rPr->first_child("u");
        if (u && to_lower(u->attr("val", "single")) != "none") underline = true;
        auto s = rPr->first_child("strike");
        if (s && s->attr("val", "1") != "0" && s->attr("val", "true") != "false") strike = true;

        if (auto va = rPr->first_child("vertAlign")) {
            std::string vv = to_lower(va->attr("val"));
            if (vv == "superscript") sup = true;
            else if (vv == "subscript") sub = true;
        }
        if (auto col = rPr->first_child("color")) {
            std::string cv = normalize_hex(col->attr("val"));
            if (!cv.empty() && cv != "000000" && cv != "111827") color_hex = "#" + cv;
        }
        if (auto hl = rPr->first_child("highlight")) {
            hl_hex = map_word_highlight_to_hex(hl->attr("val"));
        }
        std::string shd_fill;
        if (auto shd = rPr->first_child("shd")) {
            std::string fv = normalize_hex(shd->attr("fill"));
            if (!fv.empty() && fv != "FFFFFF") shd_fill = "#" + fv;
        }
        if (auto fonts = rPr->first_child("rFonts")) {
            std::string f_en = fonts->attr("ascii");
            std::string f_cn = fonts->attr("eastAsia");
            std::string fl = to_lower(f_en + " " + fonts->attr("hAnsi"));
            if (fl.find("consolas") != std::string::npos || fl.find("courier") != std::string::npos ||
                fl.find("jetbrains") != std::string::npos || fl.find("mono") != std::string::npos) {
                is_code = true;
            } else if (!is_heading) {
                if (!f_cn.empty() && f_cn != "宋体") custom_font = f_cn;
                else if (!f_en.empty() && f_en != "Times New Roman" && f_en != "Calibri") custom_font = f_en;
            }
        }
        if (auto sz = rPr->first_child("sz")) {
            if (!is_heading) {
                try {
                    double pt = std::stod(sz->attr("val")) * 0.5;
                    if (pt > 0.0 && std::abs(pt - 10.5) > 0.25 && std::abs(pt - 11.0) > 0.25) size_pt = pt;
                } catch (...) {}
            }
        }
        if (hl_hex.empty() && !shd_fill.empty() && !(is_code && shd_fill == "#F1F5F9")) {
            hl_hex = shd_fill;
        }
    }

    if (is_code && text.find('`') == std::string::npos && !bold && !italic && !underline && !strike) {
        return "`" + text + "`";
    }

    std::string out = text;
    if (is_code) out = "`" + out + "`";
    if (!is_heading) {
        if (bold && italic) out = "***" + out + "***";
        else if (bold) out = "**" + out + "**";
        else if (italic) out = "*" + out + "*";
    } else if (italic) {
        out = "*" + out + "*";
    }
    if (strike) out = "~~" + out + "~~";
    if (underline) out = "<u>" + out + "</u>";
    if (sup) out = "<sup>" + out + "</sup>";
    if (sub) out = "<sub>" + out + "</sub>";
    if (!hl_hex.empty()) out = "<mark style=\"background-color:" + hl_hex + "\">" + out + "</mark>";

    std::string span_st;
    if (!color_hex.empty()) span_st += "color:" + color_hex + ";";
    if (!custom_font.empty()) span_st += "font-family:'" + custom_font + "';";
    if (size_pt > 0.0) {
        char buf[24];
        std::snprintf(buf, sizeof(buf), "%.1fpt;", size_pt);
        span_st += std::string("font-size:") + buf;
    }
    if (!span_st.empty()) {
        out = "<span style=\"" + span_st + "\">" + out + "</span>";
    }
    return out;
}

std::string extract_paragraph_markdown(const XmlNode* p_node,
                                       const ZipArchive& zip,
                                       const std::map<std::string, std::string>& rels,
                                       bool& out_has_figma,
                                       std::string& out_figma_json,
                                       bool is_heading = false) {
    std::string content;
    for (const auto& c : p_node->children) {
        std::string ln = c->local_name();
        if (ln == "pPr" || ln == "bookmarkStart" || ln == "bookmarkEnd") continue;
        if (ln == "r") {
            content += extract_run_markdown(c.get(), zip, rels, out_has_figma, out_figma_json, is_heading);
        } else if (ln == "hyperlink") {
            std::string rid = c->attr("id");
            std::string link_text;
            for (const auto& sub : c->children) {
                if (sub->local_name() == "r") link_text += sub->inner_text();
            }
            auto it = rels.find(rid);
            if (it != rels.end() && !link_text.empty()) {
                content += "[" + link_text + "](" + it->second + ")";
            } else {
                content += link_text;
            }
        } else if (ln == "oMathPara") {
            std::string tex = trim_ws(omml_node_to_latex(c.get()));
            if (!tex.empty()) content += "$$" + tex + "$$";
        } else if (ln == "oMath") {
            std::string tex = trim_ws(omml_node_to_latex(c.get()));
            if (!tex.empty()) content += "$" + tex + "$";
        }
    }
    return trim_ws(content);
}

BlockProps extract_paragraph_props(const XmlNode* p_node, bool is_heading) {
    BlockProps bp;
    auto pPr = p_node->first_child("pPr");
    if (!pPr) return bp;

    if (auto jc = pPr->first_child("jc")) {
        std::string v = to_lower(jc->attr("val"));
        if (v == "both") v = "justify";
        if (v == "center" || v == "right" || v == "justify") bp.align = v;
    }
    if (auto sp = pPr->first_child("spacing")) {
        try {
            std::string ls = sp->attr("line");
            std::string lr = to_lower(sp->attr("lineRule", "auto"));
            if (!ls.empty()) {
                double lv = std::stod(ls);
                if (lr == "atleast" || lr == "exact") {
                    bp.line_rule = (lr == "atleast") ? "atLeast" : "exact";
                    bp.line_pt = lv / 20.0;
                } else {
                    double ratio = lv / 240.0;
                    if (ratio > 0.5 && std::abs(ratio - 1.25) > 0.04) bp.line_spacing = ratio;
                }
            }
            std::string bf = sp->attr("before");
            if (!bf.empty() && !is_heading) bp.space_before_pt = std::stod(bf) / 20.0;
            std::string af = sp->attr("after");
            if (!af.empty() && !is_heading) {
                double ap = std::stod(af) / 20.0;
                if (std::abs(ap - 6.0) > 0.5) bp.space_after_pt = ap;
            }
        } catch (...) {}
    }
    if (auto ind = pPr->first_child("ind")) {
        try {
            std::string flc = ind->attr("firstLineChars");
            if (!flc.empty()) bp.first_line_indent_chars = std::stod(flc) / 100.0;
            else if (!ind->attr("firstLine").empty()) {
                bp.first_line_indent_chars = std::stod(ind->attr("firstLine")) / 210.0;
            }
        } catch (...) {}
    }
    if (pPr->first_child("pBdr")) bp.has_border = true;
    if (auto shd = pPr->first_child("shd")) {
        std::string bg = normalize_hex(shd->attr("fill"));
        if (!bg.empty() && bg != "FFFFFF") bp.shading = "#" + bg;
    }
    return bp;
}

} // namespace

std::string latex_to_omml(const std::string& tex) {
    TexToOmml conv(tex);
    return conv.convert();
}

std::string omml_node_to_latex(const XmlNode* node) {
    return trim_ws(extract_omml_recursive(node));
}

bool markdown_to_docx(const std::string& md,
                      const std::string& out_docx_path,
                      std::string* err,
                      const DocxExportContext* ctx) {
    MdDocument doc = parse_markdown(md);
    ZipArchive zip;
    ExportMediaState ms;
    ms.zip = &zip;
    if (ctx) {
        ms.base_dir = ctx->base_dir;
        ms.diagram_pngs = &ctx->diagram_pngs;
    } else {
        fs::path out_parent = utf8_to_path(out_docx_path).parent_path();
        ms.base_dir = path_to_utf8(out_parent);
    }

    int next_bm_id = 1;
    int diagram_idx = 0;

    std::ostringstream doc_xml;
    doc_xml << R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<w:document xmlns:w="http://schemas.openxmlformats.org/wordprocessingml/2006/main"
            xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships"
            xmlns:m="http://schemas.openxmlformats.org/officeDocument/2006/math"
            xmlns:wp="http://schemas.openxmlformats.org/drawingml/2006/wordprocessingDrawing"
            xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main"
            xmlns:pic="http://schemas.openxmlformats.org/drawingml/2006/picture">
  <w:body>
)";

    for (const auto& blk : doc.blocks) {
        switch (blk.kind) {
            case MdBlock::Kind::Heading: {
                int lvl = std::max(1, std::min(6, blk.level));
                doc_xml << "    <w:p><w:pPr><w:pStyle w:val=\"Heading" << lvl << "\"/>";
                write_paragraph_ppr_extras(doc_xml, blk.props);
                doc_xml << "</w:pPr>";
                write_inlines_ooxml(doc_xml, blk.inlines, blk.props, ms);
                doc_xml << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::Paragraph: {
                doc_xml << "    <w:p>";
                if (!blk.props.empty()) {
                    doc_xml << "<w:pPr>";
                    write_paragraph_ppr_extras(doc_xml, blk.props);
                    doc_xml << "</w:pPr>";
                }
                write_inlines_ooxml(doc_xml, blk.inlines, blk.props, ms);
                doc_xml << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::Blockquote: {
                doc_xml << "    <w:p><w:pPr><w:pStyle w:val=\"Quote\"/>";
                write_paragraph_ppr_extras(doc_xml, blk.props);
                doc_xml << "</w:pPr>";
                write_inlines_ooxml(doc_xml, blk.inlines, blk.props, ms);
                doc_xml << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::BulletItem: {
                int ilvl = std::max(0, std::min(5, blk.indent_level));
                doc_xml << "    <w:p><w:pPr><w:numPr><w:ilvl w:val=\"" << ilvl << "\"/><w:numId w:val=\"1\"/></w:numPr>";
                write_paragraph_ppr_extras(doc_xml, blk.props);
                doc_xml << "</w:pPr>";
                write_inlines_ooxml(doc_xml, blk.inlines, blk.props, ms);
                doc_xml << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::OrderedItem: {
                int ilvl = std::max(0, std::min(5, blk.indent_level));
                doc_xml << "    <w:p><w:pPr><w:numPr><w:ilvl w:val=\"" << ilvl << "\"/><w:numId w:val=\"2\"/></w:numPr>";
                write_paragraph_ppr_extras(doc_xml, blk.props);
                doc_xml << "</w:pPr>";
                write_inlines_ooxml(doc_xml, blk.inlines, blk.props, ms);
                doc_xml << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::TaskItem: {
                int ilvl = std::max(0, std::min(5, blk.indent_level));
                int left_tw = 420 * (ilvl + 1);
                doc_xml << "    <w:p><w:pPr><w:ind w:left=\"" << left_tw << "\" w:hanging=\"260\"/>";
                write_paragraph_ppr_extras(doc_xml, blk.props);
                doc_xml << "</w:pPr><w:r><w:t xml:space=\"preserve\">"
                        << (blk.checked ? "☑ " : "☐ ") << "</w:t></w:r>";
                write_inlines_ooxml(doc_xml, blk.inlines, blk.props, ms);
                doc_xml << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::CodeBlock: {
                int bm_id = next_bm_id++;
                doc_xml << "    <w:p><w:pPr><w:pStyle w:val=\"CodeBlock\"/></w:pPr>";
                if (!blk.lang.empty()) {
                    doc_xml << "<w:bookmarkStart w:id=\"" << bm_id << "\" w:name=\"OMNI_CODE_"
                            << xml_escape(blk.lang) << "\"/>";
                }
                std::istringstream iss(blk.raw);
                std::string line;
                bool first = true;
                while (std::getline(iss, line)) {
                    if (!first) doc_xml << "<w:r><w:br/></w:r>";
                    doc_xml << "<w:r><w:rPr><w:rFonts w:ascii=\"Consolas\" w:eastAsia=\"宋体\" w:hAnsi=\"Consolas\"/>"
                            << "<w:sz w:val=\"19\"/></w:rPr><w:t xml:space=\"preserve\">"
                            << xml_escape(line) << "</w:t></w:r>";
                    first = false;
                }
                if (!blk.lang.empty()) {
                    doc_xml << "<w:bookmarkEnd w:id=\"" << bm_id << "\"/>";
                }
                doc_xml << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::MathBlock: {
                doc_xml << "    <w:p><w:pPr><w:jc w:val=\"center\"/>";
                write_paragraph_ppr_extras(doc_xml, blk.props);
                doc_xml << "</w:pPr><m:oMathPara>" << latex_to_omml(blk.raw) << "</m:oMathPara>"
                        << "</w:p>\n";
                break;
            }
            case MdBlock::Kind::DiagramBlock: {
                std::string png_bytes;
                if (ms.diagram_pngs) {
                    auto it = ms.diagram_pngs->find(blk.raw);
                    if (it == ms.diagram_pngs->end()) {
                        it = ms.diagram_pngs->find(trim_ws(blk.raw));
                    }
                    if (it == ms.diagram_pngs->end()) {
                        it = ms.diagram_pngs->find("fig_" + std::to_string(diagram_idx));
                    }
                    if (it != ms.diagram_pngs->end() && !it->second.empty()) {
                        std::string v = it->second;
                        size_t comma = v.find(',');
                        if (v.rfind("data:image/", 0) == 0 && comma != std::string::npos) {
                            png_bytes = base64_decode(v.substr(comma + 1));
                        } else {
                            png_bytes = base64_decode(v);
                        }
                    }
                }
                if (png_bytes.empty()) {
                    png_bytes = render_figma_fallback_png(blk.raw);
                }
                int img_id = ms.next_img_id++;
                std::string media_rel = "media/figma_" + std::to_string(img_id) + ".png";
                ms.zip->set_entry("word/" + media_rel, png_bytes);
                ms.media_exts.insert("png");
                std::string rid = "rId" + std::to_string(ms.next_rel_id++);
                ms.rels.push_back({rid, "http://schemas.openxmlformats.org/officeDocument/2006/relationships/image", media_rel, false});
                int pw = 680, ph = 360;
                read_image_dimensions(png_bytes, "png", pw, ph);
                std::string descr = "OMNI_FIGMA:" + base64_encode(blk.raw);
                doc_xml << "    <w:p><w:pPr><w:jc w:val=\"center\"/></w:pPr>"
                        << build_drawing_ooxml(rid, img_id, "FigmaDiagram" + std::to_string(diagram_idx), descr, pw, ph)
                        << "</w:p>\n";
                ++diagram_idx;
                break;
            }
            case MdBlock::Kind::Hr: {
                doc_xml << "    <w:p><w:pPr><w:pBdr><w:bottom w:val=\"single\" w:sz=\"6\" w:space=\"1\" w:color=\"CBD5E1\"/></w:pBdr></w:pPr></w:p>\n";
                break;
            }
            case MdBlock::Kind::PageBreak: {
                doc_xml << "    <w:p><w:r><w:br w:type=\"page\"/></w:r></w:p>\n";
                break;
            }
            case MdBlock::Kind::Table: {
                doc_xml << "    <w:tbl><w:tblPr><w:tblW w:w=\"9000\" w:type=\"dxa\"/>"
                        << "<w:tblBorders>"
                        << "<w:top w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"CBD5E1\"/>"
                        << "<w:left w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"CBD5E1\"/>"
                        << "<w:bottom w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"CBD5E1\"/>"
                        << "<w:right w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"CBD5E1\"/>"
                        << "<w:insideH w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"E2E8F0\"/>"
                        << "<w:insideV w:val=\"single\" w:sz=\"4\" w:space=\"0\" w:color=\"E2E8F0\"/>"
                        << "</w:tblBorders></w:tblPr>\n";
                for (size_t r = 0; r < blk.table_rows.size(); ++r) {
                    doc_xml << "      <w:tr>\n";
                    for (size_t c = 0; c < blk.table_rows[r].size(); ++c) {
                        std::string align = (c < blk.table_aligns.size()) ? blk.table_aligns[c] : "left";
                        doc_xml << "        <w:tc><w:tcPr>";
                        if (r == 0) {
                            doc_xml << "<w:shd w:val=\"clear\" w:color=\"auto\" w:fill=\"F1F5F9\"/>";
                        }
                        doc_xml << "</w:tcPr><w:p>";
                        if (align == "center" || align == "right") {
                            doc_xml << "<w:pPr><w:jc w:val=\"" << align << "\"/></w:pPr>";
                        }
                        write_inlines_ooxml(doc_xml, blk.table_rows[r][c], blk.props, ms);
                        doc_xml << "</w:p></w:tc>\n";
                    }
                    doc_xml << "      </w:tr>\n";
                }
                doc_xml << "    </w:tbl>\n";
                break;
            }
        }
    }

    doc_xml << R"(    <w:sectPr>
      <w:pgSz w:w="11906" w:h="16838"/>
      <w:pgMar w:top="1440" w:right="1440" w:bottom="1440" w:left="1440"/>
    </w:sectPr>
  </w:body>
</w:document>
)";

    zip.set_entry("[Content_Types].xml", build_content_types_xml(ms.media_exts));
    zip.set_entry("_rels/.rels", build_root_rels_xml());
    zip.set_entry("word/_rels/document.xml.rels", build_doc_rels_xml(ms.rels));
    zip.set_entry("word/styles.xml", build_styles_xml());
    zip.set_entry("word/numbering.xml", build_numbering_xml());
    zip.set_entry("word/document.xml", doc_xml.str());
    return zip.save_to_file(out_docx_path, err);
}

bool docx_to_markdown(const std::string& docx_path, std::string& out_md, std::string* err) {
    ZipArchive zip;
    if (!zip.load_from_file(docx_path, err)) return false;

    std::string doc_xml;
    if (!zip.get_entry("word/document.xml", doc_xml)) {
        if (err) *err = "word/document.xml not found inside .docx archive";
        return false;
    }

    std::map<std::string, std::string> rels;
    std::string rels_xml;
    if (zip.get_entry("word/_rels/document.xml.rels", rels_xml)) {
        auto rels_root = parse_xml(rels_xml);
        if (rels_root) {
            if (auto rels_elem = rels_root->first_child("Relationships")) {
                for (const auto& r : rels_elem->children_named("Relationship")) {
                    std::string id = r->attr("Id");
                    std::string target = r->attr("Target");
                    if (!id.empty() && !target.empty()) rels[id] = target;
                }
            }
        }
    }

    // Parse word/numbering.xml to distinguish bullet vs ordered lists
    std::map<std::string, bool> abs_is_ordered;
    std::map<std::string, bool> num_is_ordered;
    std::string num_xml;
    if (zip.get_entry("word/numbering.xml", num_xml)) {
        if (auto nroot = parse_xml(num_xml)) {
            if (auto numbering = nroot->first_child("numbering")) {
                for (const auto& an : numbering->children_named("abstractNum")) {
                    std::string aid = an->attr("abstractNumId");
                    bool ord = false;
                    if (auto lvl = an->first_child("lvl")) {
                        if (auto fmt = lvl->first_child("numFmt")) {
                            std::string fv = to_lower(fmt->attr("val"));
                            if (fv != "bullet" && fv != "none") ord = true;
                        }
                    }
                    if (!aid.empty()) abs_is_ordered[aid] = ord;
                }
                for (const auto& nm : numbering->children_named("num")) {
                    std::string nid = nm->attr("numId");
                    if (auto aid_node = nm->first_child("abstractNumId")) {
                        auto it = abs_is_ordered.find(aid_node->attr("val"));
                        if (it != abs_is_ordered.end() && !nid.empty()) {
                            num_is_ordered[nid] = it->second;
                        }
                    }
                }
            }
        }
    }

    auto root = parse_xml(doc_xml, err);
    if (!root) return false;
    auto document_elem = root->first_child("document");
    if (!document_elem) {
        if (err) *err = "Invalid word/document.xml: missing <w:document>";
        return false;
    }
    auto body = document_elem->first_child("body");
    if (!body) {
        if (err) *err = "Invalid word/document.xml: missing <w:body>";
        return false;
    }

    std::ostringstream md;
    bool first_block = true;
    auto push_line = [&](const std::string& s, bool tight = false) {
        if (!first_block) md << (tight ? "\n" : "\n\n");
        md << s;
        first_block = false;
    };

    bool prev_was_list = false;
    std::map<int, int> ord_counters;

    for (const auto& child : body->children) {
        if (child->is_text) continue;
        std::string ln = child->local_name();
        if (ln == "p") {
            std::string code_lang;
            if (is_codeblock_paragraph(child.get(), code_lang)) {
                prev_was_list = false;
                ord_counters.clear();
                std::string raw_code;
                for (const auto& rc : child->children) {
                    if (rc->local_name() != "r") continue;
                    for (const auto& rsub : rc->children) {
                        std::string rln = rsub->local_name();
                        if (rln == "t") raw_code += rsub->inner_text();
                        else if (rln == "br") raw_code += "\n";
                    }
                }
                push_line("```" + code_lang + "\n" + raw_code + "\n```");
                continue;
            }

            int heading = detect_heading_level(child.get());
            bool has_figma = false;
            std::string figma_json;
            std::string text = extract_paragraph_markdown(child.get(), zip, rels, has_figma, figma_json, heading > 0);
            if (has_figma) {
                prev_was_list = false;
                push_line("```figma\n" + figma_json + "\n```");
                continue;
            }

            // Detect explicit page break (<w:r><w:br w:type="page"/></w:r>) or horizontal rule paragraph
            bool has_page_break = false;
            for (const auto& rnode : child->children_named("r")) {
                for (const auto& brnode : rnode->children_named("br")) {
                    if (to_lower(brnode->attr("type")) == "page") {
                        has_page_break = true;
                        break;
                    }
                }
                if (has_page_break) break;
            }
            if (text.empty()) {
                if (has_page_break) {
                    prev_was_list = false;
                    push_line("<!-- pagebreak -->");
                    continue;
                }
                if (auto pPr = child->first_child("pPr")) {
                    if (auto pBdr = pPr->first_child("pBdr")) {
                        if (pBdr->first_child("bottom")) {
                            prev_was_list = false;
                            push_line("---");
                        }
                    }
                }
                continue;
            }

            BlockProps bp = extract_paragraph_props(child.get(), heading > 0);

            bool is_list = false;
            bool is_ord = false;
            int ilvl = 0;
            if (heading == 0 && !is_quote_paragraph(child.get())) {
                is_list = extract_list_info(child.get(), num_is_ordered, is_ord, ilvl);
            }

            // Detect checkbox task item prefix
            bool is_task = false;
            bool task_checked = false;
            if (!is_list && heading == 0) {
                if (text.rfind("☐ ", 0) == 0) {
                    is_task = true; task_checked = false; text = text.substr(std::string("☐ ").size());
                } else if (text.rfind("☑ ", 0) == 0) {
                    is_task = true; task_checked = true; text = text.substr(std::string("☑ ").size());
                }
            }

            // Suppress redundant center alignment directive on single display MathBlock
            bool is_display_math = (text.rfind("$$", 0) == 0 && text.rfind("$$") == text.size() - 2);
            if (is_display_math && bp.align == "center") bp.align.clear();

            std::string dir = format_block_props_directive(bp);
            std::string prefix_dir = dir.empty() ? "" : (dir + "\n");

            if (heading > 0) {
                prev_was_list = false;
                ord_counters.clear();
                push_line(prefix_dir + std::string(static_cast<size_t>(heading), '#') + " " + text);
            } else if (is_quote_paragraph(child.get())) {
                prev_was_list = false;
                ord_counters.clear();
                push_line(prefix_dir + "> " + text);
            } else if (is_task) {
                std::string indent(static_cast<size_t>(ilvl) * 2, ' ');
                push_line(prefix_dir + indent + (task_checked ? "- [x] " : "- [ ] ") + text, prev_was_list);
                prev_was_list = true;
            } else if (is_list) {
                std::string indent(static_cast<size_t>(ilvl) * 2, ' ');
                std::string list_pfx = "- ";
                if (is_ord) {
                    int& cnt = ord_counters[ilvl];
                    if (cnt <= 0) cnt = 1;
                    list_pfx = std::to_string(cnt++) + ". ";
                }
                push_line(prefix_dir + indent + list_pfx + text, prev_was_list);
                prev_was_list = true;
            } else {
                prev_was_list = false;
                ord_counters.clear();
                push_line(prefix_dir + text);
            }
        } else if (ln == "tbl") {
            prev_was_list = false;
            std::vector<std::vector<std::string>> rows;
            std::vector<std::string> col_aligns;
            for (const auto& tr : child->children_named("tr")) {
                std::vector<std::string> row;
                size_t ci = 0;
                for (const auto& tc : tr->children_named("tc")) {
                    std::string cell_text;
                    for (const auto& p : tc->children_named("p")) {
                        if (col_aligns.size() <= ci) {
                            std::string cal = "left";
                            if (auto pPr = p->first_child("pPr")) {
                                if (auto jc = pPr->first_child("jc")) {
                                    std::string jv = to_lower(jc->attr("val"));
                                    if (jv == "center" || jv == "right") cal = jv;
                                }
                            }
                            col_aligns.push_back(cal);
                        }
                        bool hf = false;
                        std::string fj;
                        std::string pt = extract_paragraph_markdown(p.get(), zip, rels, hf, fj);
                        if (!pt.empty()) {
                            if (!cell_text.empty()) cell_text += "<br/>";
                            cell_text += pt;
                        }
                    }
                    for (size_t k = 0; k < cell_text.size(); ++k) {
                        if (cell_text[k] == '|') {
                            cell_text.replace(k, 1, "\\|");
                            ++k;
                        }
                    }
                    row.push_back(cell_text);
                    ++ci;
                }
                if (!row.empty()) rows.push_back(std::move(row));
            }
            if (!rows.empty()) {
                size_t cols = 0;
                for (const auto& r : rows) cols = std::max(cols, r.size());
                std::ostringstream tbl;
                for (size_t r = 0; r < rows.size(); ++r) {
                    tbl << "|";
                    for (size_t c = 0; c < cols; ++c) {
                        tbl << " " << (c < rows[r].size() ? rows[r][c] : "") << " |";
                    }
                    tbl << "\n";
                    if (r == 0) {
                        tbl << "|";
                        for (size_t c = 0; c < cols; ++c) {
                            std::string al = (c < col_aligns.size()) ? col_aligns[c] : "left";
                            if (al == "center") tbl << " :---: |";
                            else if (al == "right") tbl << " ---: |";
                            else tbl << " :--- |";
                        }
                        tbl << "\n";
                    }
                }
                std::string tbl_str = tbl.str();
                if (!tbl_str.empty() && tbl_str.back() == '\n') tbl_str.pop_back();
                push_line(tbl_str);
            }
        }
    }

    out_md = md.str();
    return true;
}

} // namespace omni
