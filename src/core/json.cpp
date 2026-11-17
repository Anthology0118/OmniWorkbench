#include "json.hpp"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <sstream>

namespace omni {

namespace {

const JsonValue kNullValue;

void append_utf8(std::string& out, uint32_t cp) {
    if (cp <= 0x7F) {
        out.push_back(static_cast<char>(cp));
    } else if (cp <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((cp >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((cp >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp <= 0x10FFFF) {
        out.push_back(static_cast<char>(0xF0 | ((cp >> 18) & 0x07)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

class Parser {
public:
    explicit Parser(const std::string& src) : src_(src), pos_(0) {}

    bool parse_root(JsonValue& out, std::string* err) {
        skip_ws();
        if (!parse_value(out, err)) return false;
        skip_ws();
        if (pos_ != src_.size()) {
            if (err) *err = "Unexpected trailing characters at pos " + std::to_string(pos_);
            return false;
        }
        return true;
    }

private:
    const std::string& src_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                ++pos_;
            } else {
                break;
            }
        }
    }

    bool parse_value(JsonValue& out, std::string* err) {
        skip_ws();
        if (pos_ >= src_.size()) {
            if (err) *err = "Unexpected end of JSON input";
            return false;
        }
        char c = src_[pos_];
        if (c == 'n') return parse_literal("null", JsonValue(nullptr), out, err);
        if (c == 't') return parse_literal("true", JsonValue(true), out, err);
        if (c == 'f') return parse_literal("false", JsonValue(false), out, err);
        if (c == '"') {
            std::string s;
            if (!parse_string(s, err)) return false;
            out = JsonValue(std::move(s));
            return true;
        }
        if (c == '[') return parse_array(out, err);
        if (c == '{') return parse_object(out, err);
        if (c == '-' || (c >= '0' && c <= '9')) return parse_number(out, err);

        if (err) *err = std::string("Unexpected character '") + c + "' at pos " + std::to_string(pos_);
        return false;
    }

    bool parse_literal(const char* lit, const JsonValue& val, JsonValue& out, std::string* err) {
        size_t len = std::char_traits<char>::length(lit);
        if (src_.compare(pos_, len, lit) == 0) {
            pos_ += len;
            out = val;
            return true;
        }
        if (err) *err = std::string("Expected literal ") + lit;
        return false;
    }

    bool parse_hex4(uint32_t& code) {
        if (pos_ + 4 > src_.size()) return false;
        code = 0;
        for (int i = 0; i < 4; ++i) {
            char c = src_[pos_++];
            code <<= 4;
            if (c >= '0' && c <= '9') code |= static_cast<uint32_t>(c - '0');
            else if (c >= 'a' && c <= 'f') code |= static_cast<uint32_t>(10 + c - 'a');
            else if (c >= 'A' && c <= 'F') code |= static_cast<uint32_t>(10 + c - 'A');
            else return false;
        }
        return true;
    }

    bool parse_string(std::string& out, std::string* err) {
        if (pos_ >= src_.size() || src_[pos_] != '"') {
            if (err) *err = "Expected '\"'";
            return false;
        }
        ++pos_;
        out.clear();
        while (pos_ < src_.size()) {
            char c = src_[pos_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (pos_ >= src_.size()) {
                    if (err) *err = "Unterminated escape";
                    return false;
                }
                char esc = src_[pos_++];
                switch (esc) {
                    case '"': out.push_back('"'); break;
                    case '\\': out.push_back('\\'); break;
                    case '/': out.push_back('/'); break;
                    case 'b': out.push_back('\b'); break;
                    case 'f': out.push_back('\f'); break;
                    case 'n': out.push_back('\n'); break;
                    case 'r': out.push_back('\r'); break;
                    case 't': out.push_back('\t'); break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parse_hex4(cp)) {
                            if (err) *err = "Invalid unicode escape";
                            return false;
                        }
                        if (cp >= 0xD800 && cp <= 0xDBFF && pos_ + 2 <= src_.size() &&
                            src_[pos_] == '\\' && src_[pos_ + 1] == 'u') {
                            pos_ += 2;
                            uint32_t low = 0;
                            if (!parse_hex4(low)) {
                                if (err) *err = "Invalid surrogate pair";
                                return false;
                            }
                            cp = 0x10000 + (((cp - 0xD800) << 10) | (low - 0xDC00));
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default:
                        if (err) *err = "Invalid escape sequence";
                        return false;
                }
            } else {
                out.push_back(c);
            }
        }
        if (err) *err = "Unterminated string";
        return false;
    }

    bool parse_number(JsonValue& out, std::string* err) {
        size_t start = pos_;
        if (src_[pos_] == '-') ++pos_;
        while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        if (pos_ < src_.size() && src_[pos_] == '.') {
            ++pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        }
        if (pos_ < src_.size() && (src_[pos_] == 'e' || src_[pos_] == 'E')) {
            ++pos_;
            if (pos_ < src_.size() && (src_[pos_] == '+' || src_[pos_] == '-')) ++pos_;
            while (pos_ < src_.size() && std::isdigit(static_cast<unsigned char>(src_[pos_]))) ++pos_;
        }
        try {
            double val = std::stod(src_.substr(start, pos_ - start));
            out = JsonValue(val);
            return true;
        } catch (...) {
            if (err) *err = "Invalid number";
            return false;
        }
    }

    bool parse_array(JsonValue& out, std::string* err) {
        ++pos_; // skip '['
        JsonValue::Array arr;
        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == ']') {
            ++pos_;
            out = JsonValue(std::move(arr));
            return true;
        }
        while (true) {
            JsonValue elem;
            if (!parse_value(elem, err)) return false;
            arr.push_back(std::move(elem));
            skip_ws();
            if (pos_ >= src_.size()) {
                if (err) *err = "Unterminated array";
                return false;
            }
            if (src_[pos_] == ']') {
                ++pos_;
                out = JsonValue(std::move(arr));
                return true;
            }
            if (src_[pos_] != ',') {
                if (err) *err = "Expected ',' or ']' in array";
                return false;
            }
            ++pos_;
        }
    }

    bool parse_object(JsonValue& out, std::string* err) {
        ++pos_; // skip '{'
        JsonValue::Object obj;
        skip_ws();
        if (pos_ < src_.size() && src_[pos_] == '}') {
            ++pos_;
            out = JsonValue(std::move(obj));
            return true;
        }
        while (true) {
            skip_ws();
            std::string key;
            if (!parse_string(key, err)) return false;
            skip_ws();
            if (pos_ >= src_.size() || src_[pos_] != ':') {
                if (err) *err = "Expected ':' after object key";
                return false;
            }
            ++pos_;
            JsonValue val;
            if (!parse_value(val, err)) return false;
            obj[std::move(key)] = std::move(val);
            skip_ws();
            if (pos_ >= src_.size()) {
                if (err) *err = "Unterminated object";
                return false;
            }
            if (src_[pos_] == '}') {
                ++pos_;
                out = JsonValue(std::move(obj));
                return true;
            }
            if (src_[pos_] != ',') {
                if (err) *err = "Expected ',' or '}' in object";
                return false;
            }
            ++pos_;
        }
    }
};

void dump_impl(const JsonValue& v, std::string& out, int indent, int depth) {
    auto add_indent = [&](int d) {
        if (indent >= 0) {
            out.push_back('\n');
            out.append(static_cast<size_t>(indent * d), ' ');
        }
    };

    switch (v.type) {
        case JsonValue::Type::Null:
            out += "null";
            break;
        case JsonValue::Type::Bool:
            out += (v.bool_val ? "true" : "false");
            break;
        case JsonValue::Type::Number: {
            if (std::isnan(v.num_val) || std::isinf(v.num_val)) {
                out += "null";
            } else if (std::floor(v.num_val) == v.num_val && std::abs(v.num_val) < 9e15) {
                char buf[64];
                std::snprintf(buf, sizeof(buf), "%.0f", v.num_val);
                out += buf;
            } else {
                std::ostringstream oss;
                oss << v.num_val;
                out += oss.str();
            }
            break;
        }
        case JsonValue::Type::String:
            out.push_back('"');
            out += json_escape(v.str_val);
            out.push_back('"');
            break;
        case JsonValue::Type::Array: {
            out.push_back('[');
            if (!v.arr_val.empty()) {
                for (size_t i = 0; i < v.arr_val.size(); ++i) {
                    if (i > 0) out.push_back(',');
                    add_indent(depth + 1);
                    dump_impl(v.arr_val[i], out, indent, depth + 1);
                }
                add_indent(depth);
            }
            out.push_back(']');
            break;
        }
        case JsonValue::Type::Object: {
            out.push_back('{');
            if (!v.obj_val.empty()) {
                size_t idx = 0;
                for (const auto& kv : v.obj_val) {
                    if (idx++ > 0) out.push_back(',');
                    add_indent(depth + 1);
                    out.push_back('"');
                    out += json_escape(kv.first);
                    out += (indent >= 0) ? "\": " : "\":";
                    dump_impl(kv.second, out, indent, depth + 1);
                }
                add_indent(depth);
            }
            out.push_back('}');
            break;
        }
    }
}

} // namespace

std::string json_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out.push_back(static_cast<char>(c));
                }
                break;
        }
    }
    return out;
}

const JsonValue& JsonValue::get(const std::string& key) const {
    if (type != Type::Object) return kNullValue;
    auto it = obj_val.find(key);
    return it != obj_val.end() ? it->second : kNullValue;
}

bool JsonValue::has(const std::string& key) const {
    return type == Type::Object && obj_val.find(key) != obj_val.end();
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (type != Type::Object) {
        type = Type::Object;
        obj_val.clear();
    }
    return obj_val[key];
}

void JsonValue::push_back(JsonValue val) {
    if (type != Type::Array) {
        type = Type::Array;
        arr_val.clear();
    }
    arr_val.push_back(std::move(val));
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    dump_impl(*this, out, indent, 0);
    return out;
}

bool JsonValue::parse(const std::string& text, JsonValue& out, std::string* err) {
    Parser p(text);
    return p.parse_root(out, err);
}

} // namespace omni
