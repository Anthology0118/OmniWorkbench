#pragma once

#include <string>

namespace omni {

/**
 * Convert Markdown text to an A4 PDF file.
 * Uses Edge/Chromium headless print-to-pdf for high-fidelity CJK/Math/Table layout when available,
 * with a pure C++ PDF 1.4 fallback generator.
 */
bool markdown_to_pdf(const std::string& md,
                     const std::string& out_pdf_path,
                     const std::string& title = "OmniWorkbench Document",
                     std::string* err = nullptr);

/**
 * Convert a .docx file to an A4 PDF file (DOCX -> Markdown -> A4 PDF).
 */
bool docx_to_pdf(const std::string& docx_path,
                 const std::string& out_pdf_path,
                 std::string* err = nullptr);

} // namespace omni
