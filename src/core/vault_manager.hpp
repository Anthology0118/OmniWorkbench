#pragma once

#include "docx_converter.hpp"
#include "json.hpp"

#include <string>
#include <vector>

namespace omni {

struct VaultFileContent {
    std::string rel_path;
    std::string abs_path;
    std::string ext;      // ".md", ".docx", ".txt", etc.
    std::string markdown; // Editable markdown representation
    std::string html;     // Rendered Word A4 view HTML
    bool is_docx = false;
};

/**
 * Manages the physical local knowledge vault directory, 4-layer tree scanning,
 * document reading/saving, wikilink resolution, and MD/DOCX/PDF conversions.
 */
class VaultManager {
public:
    explicit VaultManager(std::string root_path = "");

    void set_root(const std::string& root_path);
    const std::string& root() const { return root_path_; }
    bool root_exists() const;
    std::string compute_signature() const;

    /**
     * Ensure vault directory exists; optionally seed starter structure if empty.
     */
    void ensure_initialized(bool seed_if_empty = true);

    /**
     * Scan real filesystem up to 4 layers deep and return structured JSON for the UI.
     */
    JsonValue scan_tree() const;

    /**
     * Read a document (.md, .txt, .docx) into markdown + A4 HTML.
     */
    bool open_document(const std::string& rel_or_abs_path, VaultFileContent& out, std::string* err = nullptr) const;

    /**
     * Save Markdown text to either .md/.txt directly or convert & write to .docx.
     */
    bool save_document(const std::string& rel_or_abs_path,
                       const std::string& markdown_content,
                       std::string* err = nullptr,
                       const DocxExportContext* ctx = nullptr) const;

    /**
     * Create a new Markdown note or directory inside the vault.
     */
    bool create_note(const std::string& rel_path, const std::string& initial_md, std::string* err = nullptr) const;
    bool create_folder(const std::string& rel_path, std::string* err = nullptr) const;

    /**
     * Resolve [[wikilink]] name to a relative path inside the vault.
     */
    std::string resolve_wikilink(const std::string& name) const;

    /**
     * Convert current note / file between MD, DOCX, and PDF inside or outside the vault.
     * target_format: "docx" | "md" | "pdf"
     */
    bool convert_document(const std::string& source_rel_or_abs,
                          const std::string& markdown_override,
                          const std::string& target_format,
                          const std::string& out_path,
                          std::string* err = nullptr,
                          const DocxExportContext* ctx = nullptr) const;

    /**
     * Append a vocabulary entry to <Vault>/English/Vocabulary.md.
     */
    bool append_vocabulary_entry(const std::string& word,
                                 const std::string& ipa,
                                 const std::string& pos,
                                 const std::string& note,
                                 std::string* err = nullptr) const;

private:
    std::string root_path_;

    std::string resolve_safe_path(const std::string& rel_or_abs) const;
};

} // namespace omni
