#pragma once

#include "core/markdown.hpp"
#include "qt_theme.hpp"

#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QRegularExpression>
#include <QScrollArea>
#include <QString>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextDocument>
#include <QTextFragment>
#include <QTextList>
#include <QTextTable>
#include <QVBoxLayout>

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

inline QString style_inline_rich_html(QString html, const ThemePalette& pal) {
    QString codeStyle = pal.is_dark
        ? QStringLiteral("<span style=\"background-color:#263042; color:#93C5FD; font-family:Consolas; font-size:11.5px;\">&nbsp;")
        : QStringLiteral("<span style=\"background-color:#EEF2F7; color:#1E293B; font-family:Consolas; font-size:11.5px;\">&nbsp;");
    html.replace(QStringLiteral("<code>"), codeStyle);
    html.replace(QStringLiteral("</code>"), QStringLiteral("&nbsp;</span>"));
    html.replace(QStringLiteral("<span class=\"ds-tag red\">"),
                 QStringLiteral("<span style=\"background-color:#FEE2E2; color:#DC2626; font-weight:bold;\">&nbsp;"));
    html.replace(QStringLiteral("<span class=\"ds-tag amber\">"),
                 QStringLiteral("<span style=\"background-color:#FEF3C7; color:#D97706; font-weight:bold;\">&nbsp;"));
    html.replace(QStringLiteral("<span class=\"ds-tag purple\">"),
                 QStringLiteral("<span style=\"background-color:#EDE9FE; color:#7C3AED; font-weight:bold;\">&nbsp;"));
    html.replace(QStringLiteral("<span class=\"ds-tag blue\">"),
                 QStringLiteral("<span style=\"background-color:#DBEAFE; color:#2563EB; font-weight:bold;\">&nbsp;"));
    html.replace(QStringLiteral("<ul>"), QStringLiteral("<ul style=\"margin-top:3px; margin-bottom:2px; margin-left:16px;\">"));
    html.replace(QStringLiteral("<li>"), QStringLiteral("<li style=\"margin-bottom:2px;\">"));
    return html;
}

class TutorialBlockCard : public QFrame {
public:
    TutorialBlockCard(const QColor& bg, const QColor& border, const QColor& accent, QWidget* parent = nullptr)
        : QFrame(parent), bg_(bg), border_(border), accent_(accent) {
        setFrameShape(QFrame::NoFrame);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath path;
        path.addRoundedRect(r, 10, 10);
        p.fillPath(path, bg_);

        if (accent_.isValid() && accent_.alpha() > 0) {
            p.save();
            p.setClipPath(path);
            p.fillRect(QRectF(r.left(), r.top(), 4.5, r.height()), accent_);
            p.restore();
        }
        p.setPen(QPen(border_, 1.0));
        p.drawPath(path);
    }

private:
    QColor bg_;
    QColor border_;
    QColor accent_;
};

class DsTutorialWidget : public QScrollArea {
public:
    explicit DsTutorialWidget(QWidget* parent = nullptr) : QScrollArea(parent) {
        setWidgetResizable(true);
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        setFrameShape(QFrame::NoFrame);

        container_ = new QWidget(this);
        container_->setStyleSheet(QStringLiteral("background: transparent;"));
        vbox_ = new QVBoxLayout(container_);
        vbox_->setContentsMargins(2, 2, 6, 4);
        vbox_->setSpacing(8);
        setWidget(container_);
    }

    void setTutorial(const std::string& rawHtml, const ThemePalette& pal) {
        last_html_ = rawHtml;
        pal_ = pal;
        rebuild();
    }

    void setPaletteTheme(const ThemePalette& pal) {
        pal_ = pal;
        if (!last_html_.empty()) rebuild();
    }

private:
    void clearLayout() {
        while (QLayoutItem* it = vbox_->takeAt(0)) {
            if (QWidget* w = it->widget()) {
                w->hide();
                w->setParent(nullptr);
                w->deleteLater();
            }
            delete it;
        }
    }

    QLabel* makeRichLabel(const QString& html, const QColor& color, bool bold = false, int px = 12) {
        auto* lbl = new QLabel();
        lbl->setTextFormat(Qt::RichText);
        lbl->setWordWrap(true);
        lbl->setTextInteractionFlags(Qt::TextSelectableByMouse);
        lbl->setOpenExternalLinks(false);
        lbl->setContentsMargins(0, 1, 0, 2);
        lbl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        lbl->setStyleSheet(QString("background: transparent; color: %1; font-size: %2px; font-weight: %3;")
            .arg(color.name()).arg(px).arg(bold ? 700 : 400));
        lbl->setText(style_inline_rich_html(html, pal_));
        return lbl;
    }

    void addLeadCard(const QString& innerHtml) {
        QColor bg = pal_.is_dark ? QColor("#18253A") : QColor("#EEF4FF");
        QColor bd = pal_.is_dark ? QColor("#2D4366") : QColor("#CFE0FC");
        QColor ac("#2563EB");
        auto* card = new TutorialBlockCard(bg, bd, ac, container_);
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(14, 10, 12, 10);
        l->addWidget(makeRichLabel(innerHtml, QColor(pal_.text_primary), false, 12));
        vbox_->addWidget(card);
    }

    void addStepCard(const QString& tone, const QString& badge, const QString& titleHtml, const QString& descHtml) {
        QColor accent("#2563EB");
        QColor badgeBg("#DBEAFE");
        if (tone == QLatin1String("green"))       { accent = QColor("#059669"); badgeBg = QColor(pal_.is_dark ? "#064E3B" : "#DCFCE7"); }
        else if (tone == QLatin1String("purple")) { accent = QColor("#7C3AED"); badgeBg = QColor(pal_.is_dark ? "#3B1E6D" : "#EDE9FE"); }
        else if (tone == QLatin1String("amber"))  { accent = QColor("#D97706"); badgeBg = QColor(pal_.is_dark ? "#45230A" : "#FEF3C7"); }
        else if (pal_.is_dark)                    { badgeBg = QColor("#1E3A5F"); }

        QColor bg = pal_.is_dark ? QColor("#1A2230") : QColor("#FAFCFF");
        QColor bd = QColor(pal_.border_line);

        auto* card = new TutorialBlockCard(bg, bd, accent, container_);
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(14, 8, 12, 9);
        l->setSpacing(4);

        auto* hRow = new QHBoxLayout();
        hRow->setSpacing(6);
        auto* bLbl = new QLabel(badge.trimmed(), card);
        bLbl->setAlignment(Qt::AlignCenter);
        bLbl->setFixedSize(20, 20);
        bLbl->setStyleSheet(QString("background:%1; color:%2; border-radius:10px; font-weight:700; font-size:11px;")
            .arg(badgeBg.name(), accent.name()));
        auto* tLbl = makeRichLabel(titleHtml, QColor(pal_.text_primary), true, 12);
        hRow->addWidget(bLbl);
        hRow->addWidget(tLbl, 1);
        l->addLayout(hRow);

        l->addWidget(makeRichLabel(descHtml, QColor(pal_.text_secondary), false, 12));
        vbox_->addWidget(card);
    }

    void addTableCard(const QString& tableInnerHtml) {
        QColor bg = QColor(pal_.bg_card);
        QColor bd = QColor(pal_.border_line);
        auto* card = new TutorialBlockCard(bg, bd, QColor("#2563EB"), container_);
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(12, 8, 10, 8);
        QString styled = QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"5\" style=\"border-collapse:collapse;\">%1</table>")
            .arg(tableInnerHtml);
        styled.replace(QStringLiteral("<th>"), QString("<th style=\"background-color:%1; border:1px solid %2; text-align:left;\">")
            .arg(pal_.is_dark ? "#222C3D" : "#F1F5F9", bd.name()));
        styled.replace(QStringLiteral("<td>"), QString("<td style=\"border:1px solid %1;\">").arg(bd.name()));
        l->addWidget(makeRichLabel(styled, QColor(pal_.text_primary), false, 11));
        vbox_->addWidget(card);
    }

    void addNoteOrPitfallCard(const QString& innerHtml, bool isPitfall) {
        QColor bg = isPitfall
            ? (pal_.is_dark ? QColor("#2C2111") : QColor("#FFFBEB"))
            : (pal_.is_dark ? QColor("#18253A") : QColor("#F0F9FF"));
        QColor bd = isPitfall
            ? (pal_.is_dark ? QColor("#92400E") : QColor("#FDE68A"))
            : (pal_.is_dark ? QColor("#2D4366") : QColor("#BAE6FD"));
        QColor ac(isPitfall ? "#D97706" : "#0284C7");
        QColor fg(isPitfall ? (pal_.is_dark ? "#FDE68A" : "#92400E") : pal_.text_secondary);

        auto* card = new TutorialBlockCard(bg, bd, ac, container_);
        auto* l = new QVBoxLayout(card);
        l->setContentsMargins(14, 9, 12, 9);
        l->addWidget(makeRichLabel(innerHtml, fg, false, 12));
        vbox_->addWidget(card);
    }

    void rebuild() {
        clearLayout();
        const QString src = QString::fromStdString(last_html_);
        bool addedAny = false;

        // 1. Lead Box
        QRegularExpression reLead(QStringLiteral("<div class=\"ds-tut-lead\">([\\s\\S]*?)</div>"));
        QRegularExpressionMatch mLead = reLead.match(src);
        if (mLead.hasMatch()) {
            addLeadCard(mLead.captured(1).trimmed());
            addedAny = true;
        }

        // 2. Step Cards
        QRegularExpression reStep(QStringLiteral(
            "<div class=\"ds-step-card tone-([a-z]+)\">"
            "<div class=\"ds-step-title\"><span class=\"ds-step-num\">(.*?)</span>([\\s\\S]*?)</div>"
            "<div class=\"ds-step-desc\">([\\s\\S]*?)</div></div>"));
        QRegularExpressionMatchIterator itSteps = reStep.globalMatch(src);
        while (itSteps.hasNext()) {
            QRegularExpressionMatch m = itSteps.next();
            addStepCard(m.captured(1), m.captured(2), m.captured(3).trimmed(), m.captured(4).trimmed());
            addedAny = true;
        }

        // 3. Optional Note Box
        QRegularExpression reNote(QStringLiteral("<div class=\"ds-note-box\">([\\s\\S]*?)</div>"));
        QRegularExpressionMatch mNote = reNote.match(src);
        if (mNote.hasMatch()) {
            addNoteOrPitfallCard(mNote.captured(1).trimmed(), false);
            addedAny = true;
        }

        // 4. Optional Table
        QRegularExpression reTable(QStringLiteral("<table[^>]*>([\\s\\S]*?)</table>"));
        QRegularExpressionMatch mTab = reTable.match(src);
        if (mTab.hasMatch()) {
            addTableCard(mTab.captured(1).trimmed());
            addedAny = true;
        }

        // 5. Pitfall Box
        QRegularExpression rePit(QStringLiteral("<div class=\"ds-pitfall-box\">([\\s\\S]*?)</div>"));
        QRegularExpressionMatch mPit = rePit.match(src);
        if (mPit.hasMatch()) {
            addNoteOrPitfallCard(mPit.captured(1).trimmed(), true);
            addedAny = true;
        }

        if (!addedAny && !src.trimmed().isEmpty()) {
            addLeadCard(src);
        }
        vbox_->addStretch(1);
    }

    QWidget* container_ = nullptr;
    QVBoxLayout* vbox_ = nullptr;
    std::string last_html_;
    ThemePalette pal_ = get_theme_palette("white");
};

} // namespace omni::qt
