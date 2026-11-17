#include "xml_doc.hpp"

#include <cctype>
#include <cstdint>

namespace omni {

namespace {

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

std::string unescape_xml_entities(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '&') {
            size_t semi = s.find(';', i + 1);
            if (semi != std::string::npos && semi - i <= 12) {
                std::string ent = s.substr(i + 1, semi - i - 1);
                if (ent == "amp") { out.push_back('&'); i = semi; continue; }
                if (ent == "lt") { out.push_back('<'); i = semi; continue; }
                if (ent == "gt") { out.push_back('>'); i = semi; continue; }
                if (ent == "quot") { out.push_back('"'); i = semi; continue; }
                if (ent == "apos") { out.push_back('\''); i = semi; continue; }
                if (!ent.empty() && ent[0] == '#') {
                    try {
                        uint32_t cp = (ent.size() > 1 && (ent[1] == 'x' || ent[1] == 'X'))
                            ? static_cast<uint32_t>(std::stoul(ent.substr(2), nullptr, 16))
                            : static_cast<uint32_t>(std::stoul(ent.substr(1), nullptr, 10));
                        append_utf8(out, cp);
                        i = semi;
                        continue;
                    } catch (...) {}
                }
            }
        }
        out.push_back(s[i]);
    }
    return out;
}

class XmlParser {
public:
    explicit XmlParser(const std::string& src) : s_(src), pos_(0) {
        // Skip UTF-8 BOM if present
        if (s_.size() >= 3 &&
            static_cast<unsigned char>(s_[0]) == 0xEF &&
            static_cast<unsigned char>(s_[1]) == 0xBB &&
            static_cast<unsigned char>(s_[2]) == 0xBF) {
            pos_ = 3;
        }
    }

    std::shared_ptr<XmlNode> parse(std::string* err) {
        auto root = std::make_shared<XmlNode>();
        root->name = "#document";
        parse_children(root, "", err);
        return root;
    }

private:
    const std::string& s_;
    size_t pos_;

    void skip_ws() {
        while (pos_ < s_.size() && std::isspace(static_cast<unsigned char>(s_[pos_]))) ++pos_;
    }

    bool starts_with(const std::string& lit) const {
        return s_.compare(pos_, lit.size(), lit) == 0;
    }

    std::string read_name() {
        size_t start = pos_;
        while (pos_ < s_.size()) {
            unsigned char c = static_cast<unsigned char>(s_[pos_]);
            if (std::isspace(c) || c == '>' || c == '/' || c == '=' || c == '<') break;
            ++pos_;
        }
        return s_.substr(start, pos_ - start);
    }

    void parse_children(const std::shared_ptr<XmlNode>& parent, const std::string& stop_tag, std::string* err) {
        while (pos_ < s_.size()) {
            size_t lt = s_.find('<', pos_);
            if (lt == std::string::npos) {
                if (pos_ < s_.size()) {
                    auto txt = std::make_shared<XmlNode>();
                    txt->is_text = true;
                    txt->text = unescape_xml_entities(s_.substr(pos_));
                    parent->children.push_back(std::move(txt));
                }
                pos_ = s_.size();
                return;
            }
            if (lt > pos_) {
                auto txt = std::make_shared<XmlNode>();
                txt->is_text = true;
                txt->text = unescape_xml_entities(s_.substr(pos_, lt - pos_));
                parent->children.push_back(std::move(txt));
            }
            pos_ = lt;

            if (starts_with("<!--")) {
                size_t end = s_.find("-->", pos_ + 4);
                pos_ = (end == std::string::npos) ? s_.size() : end + 3;
                continue;
            }
            if (starts_with("<?")) {
                size_t end = s_.find("?>", pos_ + 2);
                pos_ = (end == std::string::npos) ? s_.size() : end + 2;
                continue;
            }
            if (starts_with("<![CDATA[")) {
                size_t end = s_.find("]]>", pos_ + 9);
                auto txt = std::make_shared<XmlNode>();
                txt->is_text = true;
                if (end == std::string::npos) {
                    txt->text = s_.substr(pos_ + 9);
                    pos_ = s_.size();
                } else {
                    txt->text = s_.substr(pos_ + 9, end - (pos_ + 9));
                    pos_ = end + 3;
                }
                parent->children.push_back(std::move(txt));
                continue;
            }
            if (starts_with("<!")) {
                size_t end = s_.find('>', pos_ + 2);
                pos_ = (end == std::string::npos) ? s_.size() : end + 1;
                continue;
            }
            if (starts_with("</")) {
                pos_ += 2;
                std::string close_name = read_name();
                size_t gt = s_.find('>', pos_);
                pos_ = (gt == std::string::npos) ? s_.size() : gt + 1;
                if (close_name == stop_tag) {
                    return;
                }
                // Mismatched tag guard: unwind to matching tag if inside element
                if (!stop_tag.empty()) {
                    return;
                }
                continue;
            }

            // Opening tag
            ++pos_; // skip '<'
            std::string tag = read_name();
            if (tag.empty()) {
                if (err) *err = "Empty XML tag name";
                ++pos_;
                continue;
            }
            auto elem = std::make_shared<XmlNode>();
            elem->name = tag;

            bool self_closing = false;
            while (pos_ < s_.size()) {
                skip_ws();
                if (starts_with("/>")) {
                    pos_ += 2;
                    self_closing = true;
                    break;
                }
                if (s_[pos_] == '>') {
                    ++pos_;
                    break;
                }
                std::string attr_name = read_name();
                if (attr_name.empty()) {
                    ++pos_;
                    continue;
                }
                skip_ws();
                std::string attr_val;
                if (pos_ < s_.size() && s_[pos_] == '=') {
                    ++pos_;
                    skip_ws();
                    if (pos_ < s_.size() && (s_[pos_] == '"' || s_[pos_] == '\'')) {
                        char q = s_[pos_++];
                        size_t end = s_.find(q, pos_);
                        if (end == std::string::npos) {
                            attr_val = unescape_xml_entities(s_.substr(pos_));
                            pos_ = s_.size();
                        } else {
                            attr_val = unescape_xml_entities(s_.substr(pos_, end - pos_));
                            pos_ = end + 1;
                        }
                    }
                }
                elem->attrs.emplace_back(std::move(attr_name), std::move(attr_val));
            }

            parent->children.push_back(elem);
            if (!self_closing) {
                parse_children(elem, elem->name, err);
            }
        }
    }
};

} // namespace

std::string XmlNode::local_name() const {
    size_t colon = name.find(':');
    return colon == std::string::npos ? name : name.substr(colon + 1);
}

std::string XmlNode::attr(const std::string& key, const std::string& def) const {
    for (const auto& kv : attrs) {
        if (kv.first == key) return kv.second;
    }
    // Also match by local name if prefixed
    for (const auto& kv : attrs) {
        size_t colon = kv.first.find(':');
        if (colon != std::string::npos && kv.first.substr(colon + 1) == key) {
            return kv.second;
        }
    }
    return def;
}

std::shared_ptr<XmlNode> XmlNode::first_child(const std::string& tag_or_local) const {
    for (const auto& c : children) {
        if (!c->is_text && (c->name == tag_or_local || c->local_name() == tag_or_local)) {
            return c;
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<XmlNode>> XmlNode::children_named(const std::string& tag_or_local) const {
    std::vector<std::shared_ptr<XmlNode>> res;
    for (const auto& c : children) {
        if (!c->is_text && (c->name == tag_or_local || c->local_name() == tag_or_local)) {
            res.push_back(c);
        }
    }
    return res;
}

std::string XmlNode::inner_text() const {
    if (is_text) return text;
    std::string out;
    for (const auto& c : children) {
        out += c->inner_text();
    }
    return out;
}

std::shared_ptr<XmlNode> parse_xml(const std::string& xml, std::string* err) {
    XmlParser p(xml);
    return p.parse(err);
}

std::string xml_escape(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char c : s) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

} // namespace omni
