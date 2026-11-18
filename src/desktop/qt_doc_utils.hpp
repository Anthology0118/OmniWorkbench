#pragma once

#include "core/markdown.hpp"
#include "qt_theme.hpp"

#include <QColor>
#include <QFont>
#include <QString>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextList>
#include <QTextTable>

#include <sstream>
#include <string>

namespace omni::qt {

inline QString wrap_ds_tutorial_html(const std::string& rawHtml, const ThemePalette& pal) {
    QString css = QString(R"(
<style>
body {
    font-family: "Microsoft YaHei UI", "Segoe UI", sans-serif;
    font-size: 12.5px;
    line-height: 1.6;
    color: %1;
    background-color: %2;
    margin: 4px;
}
.ds-tut-lead {
    background-color: %3;
    border-left: 4px solid %4;
    padding: 8px 11px;
    margin-bottom: 10px;
    border-radius: 6px;
}
.ds-step-card {
    background-color: %5;
    border: 1px solid %6;
    padding: 7px 10px;
    margin-bottom: 7px;
    border-radius: 6px;
}
.ds-step-card.tone-blue   { border-left: 4px solid #2563EB; }
.ds-step-card.tone-green  { border-left: 4px solid #059669; }
.ds-step-card.tone-purple { border-left: 4px solid #7C3AED; }
.ds-step-card.tone-amber  { border-left: 4px solid #D97706; }
.ds-step-title { font-weight: bold; font-size: 12.5px; color: %1; margin-bottom: 3px; }
.ds-step-num { color: %4; margin-right: 5px; }
.ds-step-desc { color: %7; }
.ds-pitfall-box, .ds-note-box {
    background-color: %8;
    border: 1px solid %9;
    padding: 7px 10px;
    margin-top: 8px;
    border-radius: 6px;
    color: %7;
}
code {
    font-family: "Consolas", monospace;
    background-color: %10;
    padding: 1px 4px;
    font-size: 11.5px;
}
table { border-collapse: collapse; width: 100%; margin: 6px 0; }
th, td { border: 1px solid %6; padding: 4px 7px; text-align: left; }
th { background-color: %10; font-weight: bold; }
</style>
)")
        .arg(pal.text_primary, pal.bg_card, pal.blue_soft, pal.blue,
             pal.bg_panel, pal.border_line, pal.text_secondary,
             pal.amber_soft, pal.amber, pal.bg_subtle);

    return css + QStringLiteral("<body>") + QString::fromStdString(rawHtml) + QStringLiteral("</body>");
}

inline QString format_inline_fragment(const QTextFragment& frag) {
    QString text = frag.text();
    if (text.isEmpty()) return {};
    text.replace(QChar(0x2028), QStringLiteral("<br>"));

    QTextCharFormat fmt = frag.charFormat();
    if (fmt.isImageFormat()) {
        QTextImageFormat img = fmt.toImageFormat();
        return QStringLiteral("![](%1)").arg(img.name());
    }
    bool isCode = fmt.fontFixedPitch() ||
                  fmt.font().family().contains(QStringLiteral("Consolas"), Qt::CaseInsensitive);
    if (isCode && !text.trimmed().isEmpty()) {
        return QStringLiteral("`%1`").arg(text);
    }

    QString out = text;
    if (fmt.fontWeight() >= QFont::Bold && fmt.fontItalic()) {
        out = QStringLiteral("***%1***").arg(out);
    } else if (fmt.fontWeight() >= QFont::Bold) {
        out = QStringLiteral("**%1**").arg(out);
    } else if (fmt.fontItalic()) {
        out = QStringLiteral("*%1*").arg(out);
    }
    if (fmt.fontStrikeOut()) out = QStringLiteral("~~%1~~").arg(out);
    if (fmt.fontUnderline()) out = QStringLiteral("<u>%1</u>").arg(out);
    if (fmt.verticalAlignment() == QTextCharFormat::AlignSuperScript) out = QStringLiteral("<sup>%1</sup>").arg(out);
    if (fmt.verticalAlignment() == QTextCharFormat::AlignSubScript) out = QStringLiteral("<sub>%1</sub>").arg(out);
    if (fmt.hasProperty(QTextFormat::ForegroundBrush)) {
        QColor fg = fmt.foreground().color();
        QString hex = fg.name().toLower();
        if (fg.isValid() && hex != QStringLiteral("#000000") && hex != QStringLiteral("#111827") && hex != QStringLiteral("#f1f5f9")) {
            out = QStringLiteral("<font color=\"%1\">%2</font>").arg(hex, out);
        }
    }
    if (fmt.hasProperty(QTextFormat::BackgroundBrush)) {
        QColor bg = fmt.background().color();
        if (bg.isValid() && bg.alpha() > 0 && bg.name().toLower() != QStringLiteral("#ffffff")) {
            out = QStringLiteral("<mark style=\"background:%1\">%2</mark>").arg(bg.name(), out);
        }
    }
    return out;
}

inline std::string qt_document_to_markdown(const QTextDocument* doc) {
    if (!doc) return "";
    std::ostringstream oss;
    QTextBlock blk = doc->begin();
    bool prevWasEmpty = false;

    while (blk.isValid()) {
        QTextBlockFormat bf = blk.blockFormat();
        int hLevel = bf.headingLevel();

        QString lineContent;
        for (auto it = blk.begin(); !it.atEnd(); ++it) {
            lineContent += format_inline_fragment(it.fragment());
        }

        // Preserve pagebreak marker if inserted
        if (lineContent.contains(QStringLiteral("--- pagebreak ---")) || bf.pageBreakPolicy() == QTextFormat::PageBreak_AlwaysBefore) {
            if (bf.pageBreakPolicy() == QTextFormat::PageBreak_AlwaysBefore) {
                oss << "\n<!-- pagebreak -->\n\n";
            }
        }

        if (lineContent.trimmed().isEmpty()) {
            if (!prevWasEmpty) oss << "\n";
            prevWasEmpty = true;
            blk = blk.next();
            continue;
        }
        prevWasEmpty = false;

        // Alignment directive when explicit center/right/justify
        Qt::Alignment al = bf.alignment() & Qt::AlignHorizontal_Mask;
        if (al == Qt::AlignCenter) oss << "<!-- wp:align=center -->\n";
        else if (al == Qt::AlignRight) oss << "<!-- wp:align=right -->\n";
        else if (al == Qt::AlignJustify) oss << "<!-- wp:align=justify -->\n";

        if (hLevel >= 1 && hLevel <= 6) {
            oss << std::string(static_cast<size_t>(hLevel), '#') << " " << lineContent.toStdString() << "\n\n";
        } else if (blk.textList()) {
            QTextList* lst = blk.textList();
            QTextListFormat::Style st = lst->format().style();
            int indent = std::max(0, lst->format().indent() - 1);
            std::string pad(static_cast<size_t>(indent) * 2, ' ');
            if (st == QTextListFormat::ListDecimal) {
                int num = lst->itemNumber(blk) + 1;
                oss << pad << num << ". " << lineContent.toStdString() << "\n";
            } else {
                oss << pad << "- " << lineContent.toStdString() << "\n";
            }
            if (!blk.next().isValid() || blk.next().textList() != lst) {
                oss << "\n";
            }
        } else if (bf.leftMargin() >= 18.0) {
            oss << "> " << lineContent.toStdString() << "\n\n";
        } else {
            oss << lineContent.toStdString() << "\n\n";
        }
        blk = blk.next();
    }

    std::string res = oss.str();
    while (res.size() >= 2 && res[res.size() - 1] == '\n' && res[res.size() - 2] == '\n') {
        res.pop_back();
    }
    return res;
}

inline QString render_markdown_for_word_editor(const std::string& md, const std::string& docPath) {
    MdDocument doc = parse_markdown(md);
    std::string innerHtml = render_markdown_to_html(doc, false, "", docPath);
    QString qss = QStringLiteral(
        "<style>"
        "body { font-family: 'Times New Roman', '宋体', 'SimSun', serif; font-size: 10.5pt; color: #111827; line-height: 1.5; }"
        "h1, h2, h3, h4, h5, h6 { font-family: 'Times New Roman', '黑体', 'SimHei', sans-serif; font-weight: bold; color: #111827; }"
        "h1 { font-size: 22pt; margin: 4pt 0 10pt 0; border-bottom: 1px solid #CBD5E1; padding-bottom: 4pt; }"
        "h2 { font-size: 16pt; margin: 12pt 0 6pt 0; }"
        "h3 { font-size: 14pt; margin: 10pt 0 4pt 0; }"
        "h4 { font-size: 12pt; margin: 8pt 0 4pt 0; }"
        "p { margin: 5pt 0; }"
        "blockquote { font-family: 'Times New Roman', '楷体', 'KaiTi', serif; margin: 8pt 0 8pt 18pt; padding: 6pt 10pt; border-left: 3px solid #2563EB; background-color: #F8FAFC; color: #374151; }"
        "code { font-family: Consolas, monospace; background-color: #F1F5F9; padding: 1px 4px; }"
        "pre { font-family: Consolas, monospace; background-color: #F8FAFC; border: 1px solid #E2E8F0; padding: 8px 10px; font-size: 9.5pt; }"
        "table { border-collapse: collapse; width: 100%; margin: 8pt 0; }"
        "th, td { border: 1px solid #94A3B8; padding: 5px 8px; }"
        "th { background-color: #F1F5F9; font-weight: bold; }"
        "</style>"
    );
    return qss + QStringLiteral("<body>") + QString::fromStdString(innerHtml) + QStringLiteral("</body>");
}

} // namespace omni::qt
