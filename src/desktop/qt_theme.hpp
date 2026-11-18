#pragma once

#include <QColor>
#include <QGraphicsDropShadowEffect>
#include <QString>
#include <QWidget>
#include <string>

namespace omni::qt {

inline void apply_card_shadow(QWidget* w, bool is_dark, int blur = 22, int dy = 4, int alpha = 22) {
    if (!w) return;
    auto* eff = qobject_cast<QGraphicsDropShadowEffect*>(w->graphicsEffect());
    if (!eff) {
        eff = new QGraphicsDropShadowEffect(w);
        w->setGraphicsEffect(eff);
    }
    eff->setBlurRadius(blur);
    eff->setOffset(0, dy);
    eff->setColor(is_dark ? QColor(0, 0, 0, std::min(140, alpha * 4))
                          : QColor(15, 23, 42, alpha));
}

struct ThemePalette {
    std::string id;
    QString bg_app;
    QString bg_panel;
    QString bg_card;
    QString bg_subtle;
    QString bg_active;
    QString text_active;
    QString text_primary;
    QString text_secondary;
    QString text_muted;
    QString border_line;
    QString blue;
    QString blue_soft;
    QString emerald;
    QString emerald_soft;
    QString purple;
    QString purple_soft;
    QString amber;
    QString amber_soft;
    QString red;
    QString red_soft;
    QString code_bg;
    QString code_gutter;
    QString code_border;
    QString code_text;
    QString code_ln;
    QString paper_bg;
    bool is_dark = false;
};

inline ThemePalette get_theme_palette(const std::string& theme) {
    if (theme == "sepia" || theme == "brown") {
        return {
            "sepia",
            "#F5EFE6", "#FBF7F0", "#FFFDF9", "#EFE6D8", "#4A3423", "#FFFDF9",
            "#2C1E14", "#5C4736", "#8B735B", "#DFD2C0",
            "#B45309", "rgba(180,83,9,0.12)",
            "#047857", "rgba(4,120,87,0.12)",
            "#7C3AED", "rgba(124,58,237,0.11)",
            "#D97706", "rgba(217,119,6,0.14)",
            "#DC2626", "rgba(220,38,38,0.12)",
            "#1E1915", "#171310", "#352C24", "#F3EDE4", "#7C6D5E",
            "#FFFDF9", false
        };
    }
    if (theme == "night" || theme == "dark") {
        return {
            "night",
            "#0F141C", "#161D28", "#1C2432", "#242E3E", "#3B82F6", "#FFFFFF",
            "#F1F5F9", "#CBD5E1", "#94A3B8", "#2B3648",
            "#60A5FA", "rgba(96,165,250,0.16)",
            "#34D399", "rgba(52,211,153,0.15)",
            "#A78BFA", "rgba(167,139,250,0.16)",
            "#FBBF24", "rgba(251,191,36,0.16)",
            "#F87171", "rgba(248,113,113,0.16)",
            "#11161F", "#0D1118", "#263040", "#E5E7EB", "#64748B",
            "#FFFFFF", true
        };
    }
    // default "white" (纯白墨黑)
    return {
        "white",
        "#EEF2F7", "#F8FAFC", "#FFFFFF", "#F1F5F9", "#111827", "#FFFFFF",
        "#111827", "#374151", "#64748B", "#DCE3EE",
        "#2563EB", "rgba(37,99,235,0.11)",
        "#059669", "rgba(5,150,105,0.11)",
        "#7C3AED", "rgba(124,58,237,0.11)",
        "#D97706", "rgba(217,119,6,0.13)",
        "#DC2626", "rgba(220,38,38,0.11)",
        "#14171C", "#0F1218", "#262B35", "#E5E7EB", "#5B6474",
        "#FFFFFF", false
    };
}

inline QString build_qt_stylesheet(const ThemePalette& t) {
    QString s;
    s += QString(R"(
QMainWindow, QWidget#RootCentral {
    background-color: %1;
    color: %2;
    font-family: "Microsoft YaHei UI", "Segoe UI", "PingFang SC", sans-serif;
    font-size: 13px;
}
QWidget#TopHeaderBar {
    background-color: %3;
    border-bottom: 1px solid %4;
}
QWidget#LeftNavRail {
    background-color: %3;
    border-right: 1px solid %4;
}
QFrame[card="true"] {
    background-color: %5;
    border: 1px solid %4;
    border-radius: 13px;
}
QFrame#DsPracticeCard {
    background-color: %5;
    border: 1.5px solid %9;
    border-radius: 13px;
}
QFrame[panel="true"] {
    background-color: %3;
    border: 1px solid %4;
    border-radius: 11px;
}
QWidget#CardHeader {
    background-color: %3;
    border-bottom: 1px solid %4;
    border-top-left-radius: 12px;
    border-top-right-radius: 12px;
}
QLabel[muted="true"] {
    color: %6;
}
QLabel[secondary="true"] {
    color: %7;
}
QLabel[pill="blue"] {
    background-color: %8;
    color: %9;
    border-radius: 10px;
    padding: 2px 9px;
    font-size: 11px;
    font-weight: 600;
}
QLabel[pill="green"] {
    background-color: %10;
    color: %11;
    border-radius: 10px;
    padding: 2px 9px;
    font-size: 11px;
    font-weight: 600;
}
QLabel[pill="purple"] {
    background-color: %12;
    color: %13;
    border-radius: 10px;
    padding: 2px 9px;
    font-size: 11px;
    font-weight: 600;
}
QLabel[pill="amber"] {
    background-color: %14;
    color: %15;
    border-radius: 10px;
    padding: 2px 9px;
    font-size: 11px;
    font-weight: 600;
}
)").arg(t.bg_app, t.text_primary, t.bg_panel, t.border_line, t.bg_card,
        t.text_muted, t.text_secondary, t.blue_soft, t.blue,
        t.emerald_soft, t.emerald, t.purple_soft, t.purple, t.amber_soft, t.amber);

    s += QString(R"(
QLabel[pill="red"] {
    background-color: %1;
    color: %2;
    border-radius: 10px;
    padding: 2px 9px;
    font-size: 11px;
    font-weight: 600;
}
)").arg(t.red_soft, t.red);

    s += QString(R"(
QToolButton[nav="true"] {
    background: transparent;
    color: %1;
    border: 1px solid transparent;
    border-radius: 12px;
    padding: 8px 4px;
    font-size: 11px;
    font-weight: 600;
}
QToolButton[nav="true"]:hover {
    background-color: %2;
    color: %3;
}
QToolButton[nav="true"]:checked {
    background-color: %4;
    color: %5;
    border-color: %4;
}
QPushButton {
    background-color: %9;
    color: %3;
    border: 1px solid %6;
    border-radius: 9px;
    padding: 5px 12px;
    font-size: 12px;
    font-weight: 600;
}
QPushButton:hover {
    background-color: %2;
    border-color: %7;
    color: %7;
}
QPushButton:disabled {
    opacity: 0.45;
}
QPushButton[btn="primary"] {
    background-color: %4;
    color: %5;
    border-color: %4;
}
QPushButton[btn="primary"]:hover {
    background-color: %7;
    border-color: %7;
    color: #FFFFFF;
}
QPushButton[btn="blue"] {
    background-color: %7;
    color: #FFFFFF;
    border-color: %7;
}
QPushButton[btn="blue"]:hover {
    background-color: #1D4ED8;
    border-color: #1D4ED8;
    color: #FFFFFF;
}
QPushButton[btn="green"] {
    background-color: %8;
    color: #FFFFFF;
    border-color: %8;
}
QPushButton[btn="green"]:hover {
    background-color: #047857;
    border-color: #047857;
    color: #FFFFFF;
}
QPushButton[btn="pill"], QToolButton[pill="true"] {
    background-color: %9;
    color: %1;
    border: 1px solid %6;
    border-radius: 8px;
    padding: 4px 9px;
    font-size: 11.5px;
    font-weight: 600;
}
QPushButton[btn="pill"]:hover, QToolButton[pill="true"]:hover {
    background-color: %2;
    border-color: %7;
    color: %7;
}
QPushButton[btn="pill"]:checked, QToolButton[pill="true"]:checked,
QPushButton[btn="seg"]:checked {
    background-color: %4;
    color: %5;
    border-color: %4;
}
QPushButton[btn="seg"] {
    background-color: %2;
    color: %1;
    border: 1px solid transparent;
    border-radius: 7px;
    padding: 4px 10px;
    font-size: 11.5px;
    font-weight: 600;
}
QPushButton[btn="seg"]:hover {
    color: %3;
    background-color: %9;
}
)").arg(t.text_secondary, t.bg_subtle, t.text_primary, t.bg_active, t.text_active,
        t.border_line, t.blue, t.emerald, t.bg_card);

    s += QString(R"(
QLineEdit, QComboBox, QSpinBox {
    background-color: %1;
    color: %2;
    border: 1px solid %3;
    border-radius: 8px;
    padding: 5px 9px;
    font-size: 12px;
}
QLineEdit:focus, QComboBox:focus, QSpinBox:focus {
    border-color: %4;
}
QComboBox::drop-down {
    border: none;
    width: 20px;
}
QComboBox QAbstractItemView {
    background-color: %1;
    color: %2;
    border: 1px solid %3;
    border-radius: 8px;
    selection-background-color: %6;
    selection-color: %4;
    outline: none;
}
QSlider::groove:horizontal {
    height: 5px;
    background: %5;
    border: 1px solid %3;
    border-radius: 2px;
}
QSlider::sub-page:horizontal {
    background: %4;
    border-radius: 2px;
}
QSlider::handle:horizontal {
    background: %4;
    border: 2px solid #FFFFFF;
    width: 14px;
    height: 14px;
    margin: -5px 0;
    border-radius: 8px;
}
QTreeWidget, QListWidget {
    background-color: transparent;
    color: %2;
    border: none;
    outline: none;
    font-size: 12.5px;
    show-decoration-selected: 0;
}
QTreeWidget::item, QListWidget::item {
    border: none;
    outline: none;
}
QTextBrowser, QPlainTextEdit#MdEditor {
    background-color: %1;
    color: %2;
    border: none;
}
QSplitter::handle {
    background: transparent;
}
QSplitter::handle:horizontal { width: 8px; }
QSplitter::handle:vertical { height: 8px; }
QScrollArea {
    border: none;
    background: transparent;
}
QScrollBar:vertical {
    background: transparent;
    width: 7px;
    margin: 2px;
}
QScrollBar::handle:vertical {
    background: rgba(148,163,184,0.45);
    border-radius: 3px;
    min-height: 24px;
}
QScrollBar::handle:vertical:hover {
    background: rgba(100,116,139,0.75);
}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
    background: none;
    height: 0px;
}
QScrollBar:horizontal {
    background: transparent;
    height: 7px;
    margin: 2px;
}
QScrollBar::handle:horizontal {
    background: rgba(148,163,184,0.45);
    border-radius: 3px;
    min-width: 24px;
}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal,
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
    background: none;
    width: 0px;
}
)").arg(t.bg_card, t.text_primary, t.border_line, t.blue, t.bg_subtle, t.blue_soft);

    s += QString(R"(
QTabWidget::pane {
    border: none;
    background: transparent;
}
QTabBar::tab {
    background: %1;
    color: %2;
    border: 1px solid %3;
    border-radius: 9px;
    padding: 6px 14px;
    margin-right: 6px;
    margin-bottom: 4px;
    font-size: 12px;
    font-weight: 600;
}
QTabBar::tab:hover {
    border-color: %8;
    color: %8;
}
QTabBar::tab:selected {
    background: %4;
    color: %5;
    border-color: %4;
}
QPlainTextEdit#CodeEditor {
    background-color: %6;
    color: %7;
    border: none;
    font-family: "Consolas", "Cascadia Mono", monospace;
    font-size: 12.5px;
    selection-background-color: #264F78;
}
)").arg(t.bg_card, t.text_secondary, t.border_line, t.bg_active, t.text_active,
        t.code_bg, t.code_text, t.blue);

    return s;
}

} // namespace omni::qt
