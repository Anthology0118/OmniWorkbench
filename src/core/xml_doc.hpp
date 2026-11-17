#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace omni {

/**
 * Lightweight XML DOM node for OpenXML (.docx) parsing and manipulation.
 */
struct XmlNode {
    std::string name; // Empty if text node
    std::string text;
    bool is_text = false;
    std::vector<std::pair<std::string, std::string>> attrs;
    std::vector<std::shared_ptr<XmlNode>> children;

    std::string local_name() const;
    std::string attr(const std::string& key, const std::string& def = "") const;
    std::shared_ptr<XmlNode> first_child(const std::string& tag_or_local) const;
    std::vector<std::shared_ptr<XmlNode>> children_named(const std::string& tag_or_local) const;
    std::string inner_text() const;
};

std::shared_ptr<XmlNode> parse_xml(const std::string& xml, std::string* err = nullptr);
std::string xml_escape(const std::string& s);

} // namespace omni
