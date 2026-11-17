#pragma once

#include <map>
#include <string>

namespace omni {

struct DocxExportContext {
    std::string base_dir;                         // Base directory to resolve relative image paths
    std::map<std::string, std::string> diagram_pngs; // diagram JSON or index key -> PNG base64 / data URL
};

/**
 * Convert Markdown text directly into a standard Microsoft Word / WPS (.docx) file.
 * Supports Headings H1-H6 (黑体), Body (宋体/Times New Roman 五号), Quote (楷体),
 * Bold/Italic/Underline/Strike/Sup/Sub/Color/Highlight/Font/Size,
 * Native Word Lists (numPr/numbering.xml) & Task Lists, CodeBlocks, Images,
 * Paragraph alignment/line-spacing/indent/borders, Tables with alignment,
 * Figma Algorithm Diagrams, and LaTeX <-> Word native OMML (<m:oMath>) equations.
 */
bool markdown_to_docx(const std::string& md,
                      const std::string& out_docx_path,
                      std::string* err = nullptr,
                      const DocxExportContext* ctx = nullptr);

/**
 * Convert a standard Microsoft Word / WPS (.docx) file into Markdown text.
 */
bool docx_to_markdown(const std::string& docx_path, std::string& out_md, std::string* err = nullptr);

/**
 * Convert a LaTeX math expression into Word OMML (<m:oMath>...</m:oMath>).
 */
std::string latex_to_omml(const std::string& tex);

/**
 * Convert a Word OMML (<m:oMath> / <m:oMathPara>) XML node back into LaTeX text.
 */
std::string omml_node_to_latex(const struct XmlNode* node);

} // namespace omni
