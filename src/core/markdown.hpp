#pragma once

#include <string>
#include <vector>

namespace omni {

struct InlineSpan {
    enum class Kind {
        Text,
        Link,
        Wikilink,
        InlineMath,
        Image,
        LineBreak
    };
    Kind kind = Kind::Text;
    std::string text;
    std::string url; // used for Link / Wikilink / Image src

    // Composable inline formatting attributes (WPS / Word / MD unified)
    bool bold = false;
    bool italic = false;
    bool strike = false;
    bool underline = false;
    bool sup = false;
    bool sub = false;
    bool code = false;
    std::string color;       // hex #RRGGBB
    std::string highlight;   // hex #RRGGBB or CSS color
    std::string font_family; // e.g. "宋体", "黑体", "Times New Roman", "Consolas"
    double font_size_pt = 0.0;
};

struct BlockProps {
    std::string align;                  // "", "left", "center", "right", "justify"
    double line_spacing = 0.0;          // multiplier when line_rule == ""/"auto", e.g. 1.0, 1.15, 1.5, 2.0
    std::string line_rule;              // "", "auto", "atLeast", "exact"
    double line_pt = 0.0;               // pt value when line_rule == "atLeast" or "exact" (e.g. 23.0)
    double space_before_pt = -1.0;      // < 0 = inherit style
    double space_after_pt = -1.0;       // < 0 = inherit style
    double first_line_indent_chars = 0; // e.g. 2.0 (首行缩进2字符)
    bool has_border = false;
    std::string shading;                // hex #RRGGBB
    std::string font_family;            // optional block-wide font override
    double font_size_pt = 0.0;          // optional block-wide size override

    bool empty() const {
        return align.empty() && line_spacing <= 0.0 && line_rule.empty() && line_pt <= 0.0 &&
               space_before_pt < 0.0 && space_after_pt < 0.0 && first_line_indent_chars <= 0.0 &&
               !has_border && shading.empty() && font_family.empty() && font_size_pt <= 0.0;
    }
};

struct MdBlock {
    enum class Kind {
        Heading,
        Paragraph,
        Blockquote,
        BulletItem,
        OrderedItem,
        TaskItem,
        CodeBlock,
        MathBlock,
        DiagramBlock,
        Table,
        Hr,
        PageBreak
    };
    Kind kind = Kind::Paragraph;
    int level = 1;        // Heading 1..6, or OrderedItem number
    int indent_level = 0; // Nested list level 0, 1, 2...
    bool checked = false; // TaskItem state
    std::string raw;      // CodeBlock / MathBlock / DiagramBlock JSON
    std::string lang;     // CodeBlock language (or "figma")
    std::vector<InlineSpan> inlines;
    BlockProps props;

    // Table fields
    std::vector<std::vector<std::vector<InlineSpan>>> table_rows; // row 0 = header
    std::vector<std::string> table_aligns; // "left", "center", "right"
};

struct MdDocument {
    std::vector<MdBlock> blocks;
};

/**
 * Parse Markdown text into a structured AST.
 */
MdDocument parse_markdown(const std::string& md);

/**
 * Parse inline Markdown spans (bold, italic, underline, strike, sup, sub,
 * color, highlight, font, code, links, images, [[wikilinks]], $math$, <br>).
 */
std::vector<InlineSpan> parse_inline_markdown(const std::string& text);

/**
 * Render Markdown AST to HTML fragment for the A4 Word View or standalone A4 PDF HTML.
 */
std::string render_markdown_to_html(const MdDocument& doc,
                                    bool standalone_a4 = false,
                                    const std::string& title = "OmniWorkbench Document",
                                    const std::string& asset_doc_path = "");

/**
 * Render a LaTeX math expression into rich offline HTML/CSS (fractions, roots, matrices, scripts).
 */
std::string render_tex_to_html(const std::string& tex, bool display_mode = false);

/**
 * Render a Figma diagram JSON spec into crisp standalone SVG markup.
 */
std::string render_figma_json_to_svg(const std::string& json_str);

/**
 * Extract LaTeX formulas found in the Markdown document.
 */
std::vector<std::string> extract_latex_blocks(const std::string& md);

/**
 * Format / parse <!-- wp:... --> block property directives.
 */
std::string format_block_props_directive(const BlockProps& props);
bool parse_block_props_directive(const std::string& line, BlockProps& out_props);

std::string html_escape(const std::string& s);

} // namespace omni
