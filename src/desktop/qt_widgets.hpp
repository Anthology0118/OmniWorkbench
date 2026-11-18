#pragma once

#include "core/json.hpp"
#include "qt_theme.hpp"

#include <QAbstractScrollArea>
#include <QFont>
#include <QFontMetrics>
#include <QFrame>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPlainTextEdit>
#include <QRectF>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QStyleOptionViewItem>
#include <QStyledItemDelegate>
#include <QSyntaxHighlighter>
#include <QTextBlock>
#include <QTextFormat>
#include <QTimer>
#include <QWheelEvent>
#include <QWidget>

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace omni::qt {

// ============================================================================
// 1. Native C/C++17 Syntax Highlighter
// ============================================================================
class CppSyntaxHighlighter : public QSyntaxHighlighter {
public:
    explicit CppSyntaxHighlighter(QTextDocument* parent = nullptr)
        : QSyntaxHighlighter(parent) {
        Rule r;

        // Preprocessor (#include, #define, etc.)
        fmt_pp_.setForeground(QColor("#F472B6"));
        r.pattern = QRegularExpression(QStringLiteral("^\\s*#[^\\n]*"));
        r.format = fmt_pp_;
        rules_.push_back(r);

        // Keywords
        fmt_kw_.setForeground(QColor("#C084FC"));
        fmt_kw_.setFontWeight(QFont::Bold);
        const QStringList kws = {
            "if", "else", "for", "while", "do", "switch", "case", "default",
            "break", "continue", "return", "struct", "class", "using", "namespace",
            "const", "static", "explicit", "sizeof", "typedef", "new", "delete",
            "true", "false", "nullptr", "NULL", "auto", "public", "private", "inline"
        };
        for (const QString& kw : kws) {
            r.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(kw));
            r.format = fmt_kw_;
            rules_.push_back(r);
        }

        // Types & STL
        fmt_ty_.setForeground(QColor("#60A5FA"));
        const QStringList tys = {
            "int", "long", "double", "float", "char", "bool", "void", "unsigned",
            "string", "vector", "queue", "stack", "deque", "priority_queue",
            "set", "map", "unordered_map", "unordered_set", "pair",
            "Node", "ListNode", "Edge", "bign", "Fraction", "BIT", "DSU", "SegTree", "ull"
        };
        for (const QString& ty : tys) {
            r.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(ty));
            r.format = fmt_ty_;
            rules_.push_back(r);
        }

        // Builtin / Common Algorithm Functions
        fmt_fn_.setForeground(QColor("#34D399"));
        const QStringList fns = {
            "main", "scanf", "printf", "gets", "puts", "getchar", "putchar",
            "cin", "cout", "getline", "sort", "lower_bound", "upper_bound",
            "reverse", "swap", "min", "max", "memset", "strlen", "strcmp", "strcpy",
            "sscanf", "sprintf", "malloc", "free", "push_back", "pop_back",
            "push", "pop", "top", "front", "back", "empty", "size", "begin", "end",
            "find", "insert", "erase", "iota", "findFather", "DFS", "BFS", "DP",
            "downAdjust", "reverseList", "lowbit", "build", "pushDown", "update", "query"
        };
        for (const QString& fn : fns) {
            r.pattern = QRegularExpression(QStringLiteral("\\b%1\\b").arg(fn));
            r.format = fmt_fn_;
            rules_.push_back(r);
        }

        // Numbers
        fmt_num_.setForeground(QColor("#FB923C"));
        r.pattern = QRegularExpression(QStringLiteral("\\b[0-9]+(\\.[0-9]+)?([eE][+-]?[0-9]+)?(LL|ll|ULL|ull)?\\b"));
        r.format = fmt_num_;
        rules_.push_back(r);

        // Strings & chars
        fmt_str_.setForeground(QColor("#FBBF24"));
        r.pattern = QRegularExpression(QStringLiteral("\"([^\"\\\\]|\\\\.)*\"|'([^'\\\\]|\\\\.)*'"));
        r.format = fmt_str_;
        rules_.push_back(r);

        // Single-line comments
        fmt_cm_.setForeground(QColor("#6B7280"));
        fmt_cm_.setFontItalic(true);
        r.pattern = QRegularExpression(QStringLiteral("//[^\\n]*"));
        r.format = fmt_cm_;
        rules_.push_back(r);

        comment_start_ = QRegularExpression(QStringLiteral("/\\*"));
        comment_end_   = QRegularExpression(QStringLiteral("\\*/"));
    }

protected:
    void highlightBlock(const QString& text) override {
        for (const Rule& r : rules_) {
            QRegularExpressionMatchIterator it = r.pattern.globalMatch(text);
            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                setFormat(m.capturedStart(), m.capturedLength(), r.format);
            }
        }
        setCurrentBlockState(0);
        int startIdx = 0;
        if (previousBlockState() != 1) {
            startIdx = text.indexOf(comment_start_);
        }
        while (startIdx >= 0) {
            QRegularExpressionMatch endMatch = comment_end_.match(text, startIdx);
            int endIdx = endMatch.hasMatch() ? endMatch.capturedStart() : -1;
            int len = 0;
            if (endIdx == -1) {
                setCurrentBlockState(1);
                len = text.length() - startIdx;
            } else {
                len = endIdx - startIdx + endMatch.capturedLength();
            }
            setFormat(startIdx, len, fmt_cm_);
            startIdx = text.indexOf(comment_start_, startIdx + len);
        }
    }

private:
    struct Rule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };
    std::vector<Rule> rules_;
    QTextCharFormat fmt_pp_, fmt_kw_, fmt_ty_, fmt_fn_, fmt_num_, fmt_str_, fmt_cm_;
    QRegularExpression comment_start_, comment_end_;
};

// ============================================================================
// 2. Native IDE CodeEditor with Vertical Line-Number Gutter
// ============================================================================
class CodeEditor : public QPlainTextEdit {
public:
    std::function<void()> onCtrlEnter;
    std::function<void()> onTextEditedByUser;

    explicit CodeEditor(QWidget* parent = nullptr)
        : QPlainTextEdit(parent) {
        setObjectName(QStringLiteral("CodeEditor"));
        gutter_ = new Gutter(this);
        highlighter_ = new CppSyntaxHighlighter(document());

        QFont mono(QStringLiteral("Consolas"));
        mono.setStyleHint(QFont::Monospace);
        mono.setPointSize(10);
        setFont(mono);
        setTabStopDistance(fontMetrics().horizontalAdvance(QLatin1Char(' ')) * 4);
        setLineWrapMode(QPlainTextEdit::NoWrap);

        connect(this, &QPlainTextEdit::blockCountChanged, this, [this](int) { updateGutterWidth(); });
        connect(this, &QPlainTextEdit::updateRequest, this, [this](const QRect& rect, int dy) {
            if (dy) gutter_->scroll(0, dy);
            else gutter_->update(0, rect.y(), gutter_->width(), rect.height());
            if (rect.contains(viewport()->rect())) updateGutterWidth();
        });
        connect(this, &QPlainTextEdit::cursorPositionChanged, this, [this]() { highlightCurrentLine(); });
        connect(this, &QPlainTextEdit::textChanged, this, [this]() {
            if (!programmatic_update_ && onTextEditedByUser) onTextEditedByUser();
        });

        updateGutterWidth();
        highlightCurrentLine();
    }

    void setCodeSafely(const QString& code) {
        programmatic_update_ = true;
        setPlainText(code);
        programmatic_update_ = false;
        updateGutterWidth();
    }

    int gutterWidth() const {
        int digits = 2;
        int maxB = std::max(1, blockCount());
        while (maxB >= 10) { maxB /= 10; ++digits; }
        return 14 + fontMetrics().horizontalAdvance(QLatin1Char('9')) * std::max(2, digits);
    }

protected:
    void resizeEvent(QResizeEvent* e) override {
        QPlainTextEdit::resizeEvent(e);
        QRect cr = contentsRect();
        gutter_->setGeometry(QRect(cr.left(), cr.top(), gutterWidth(), cr.height()));
    }

    void keyPressEvent(QKeyEvent* e) override {
        if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && (e->modifiers() & Qt::ControlModifier)) {
            if (onCtrlEnter) onCtrlEnter();
            return;
        }
        if (e->key() == Qt::Key_Tab && !isReadOnly()) {
            insertPlainText(QStringLiteral("    "));
            return;
        }
        QPlainTextEdit::keyPressEvent(e);
    }

private:
    class Gutter : public QWidget {
    public:
        explicit Gutter(CodeEditor* ed) : QWidget(ed), editor_(ed) {}
        QSize sizeHint() const override { return QSize(editor_->gutterWidth(), 0); }
    protected:
        void paintEvent(QPaintEvent* event) override { editor_->paintGutter(event); }
    private:
        CodeEditor* editor_;
    };

    void updateGutterWidth() {
        setViewportMargins(gutterWidth(), 0, 0, 0);
    }

    void highlightCurrentLine() {
        QList<QTextEdit::ExtraSelection> extra;
        if (!isReadOnly()) {
            QTextEdit::ExtraSelection sel;
            sel.format.setBackground(QColor(QStringLiteral("#1D232E")));
            sel.format.setProperty(QTextFormat::FullWidthSelection, true);
            sel.cursor = textCursor();
            sel.cursor.clearSelection();
            extra.append(sel);
        }
        setExtraSelections(extra);
    }

    void paintGutter(QPaintEvent* event) {
        QPainter p(gutter_);
        p.fillRect(event->rect(), QColor(QStringLiteral("#101216")));
        p.setPen(QColor(QStringLiteral("#262B35")));
        p.drawLine(gutter_->width() - 1, event->rect().top(), gutter_->width() - 1, event->rect().bottom());

        QTextBlock block = firstVisibleBlock();
        int blockNumber = block.blockNumber();
        int top = static_cast<int>(std::round(blockBoundingGeometry(block).translated(contentOffset()).top()));
        int bottom = top + static_cast<int>(std::round(blockBoundingRect(block).height()));
        int curBlock = textCursor().blockNumber();

        p.setFont(font());
        while (block.isValid() && top <= event->rect().bottom()) {
            if (block.isVisible() && bottom >= event->rect().top()) {
                p.setPen(blockNumber == curBlock ? QColor("#93C5FD") : QColor("#5B6474"));
                p.drawText(0, top, gutter_->width() - 7, fontMetrics().height(),
                           Qt::AlignRight | Qt::AlignVCenter, QString::number(blockNumber + 1));
            }
            block = block.next();
            top = bottom;
            bottom = top + static_cast<int>(std::round(blockBoundingRect(block).height()));
            ++blockNumber;
        }
    }

    Gutter* gutter_ = nullptr;
    CppSyntaxHighlighter* highlighter_ = nullptr;
    bool programmatic_update_ = false;
};

// ============================================================================
// 3. Interactive 3D Keyboard / Phonetic Cube Widget (QPainter 3D Projection)
// ============================================================================
class Cube3DWidget : public QWidget {
public:
    std::function<void(const QString&, bool isPhonetic)> onCellSelected;

    explicit Cube3DWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(230);
        setMouseTracking(true);
        setCursor(Qt::OpenHandCursor);

        timer_ = new QTimer(this);
        connect(timer_, &QTimer::timeout, this, [this]() {
            if (auto_rotate_ && !dragging_) {
                yaw_ += 0.018;
                update();
            }
        });
    }

    void setPaletteTheme(const ThemePalette& pal) { pal_ = pal; update(); }
    void setMode(int order) { mode_order_ = (order == 2 ? 2 : 3); selected_label_.clear(); update(); }
    void setSelectedLabel(const QString& lbl) { selected_label_ = lbl; update(); }
    void toggleAutoRotate() {
        auto_rotate_ = !auto_rotate_;
        if (auto_rotate_) timer_->start(32); else timer_->stop();
        update();
    }
    void resetCamera() { yaw_ = -0.58; pitch_ = 0.44; zoom_ = 1.0; update(); }
    bool isAutoRotating() const { return auto_rotate_; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF r = rect().adjusted(2, 2, -2, -2);
        QPainterPath bgPath;
        bgPath.addRoundedRect(r, 12, 12);
        p.fillPath(bgPath, QColor(pal_.is_dark ? "#131924" : "#F8FAFC"));
        p.setPen(QPen(QColor(pal_.border_line), 1));
        p.drawPath(bgPath);

        struct FaceTile {
            double depth = 0.0;
            QPolygonF poly;
            QPointF center;
            QColor color;
            QString label;
        };

        std::vector<FaceTile> tiles;
        const QStringList letters = {
            "Q","W","E","R","T","Y","U","I","O",
            "P","A","S","D","F","G","H","J","K",
            "L","Z","X","C","V","B","N","M","*"
        };
        const QStringList ipas = {
            "/i:/","/u:/","/e/","/æ/","/ʌ/","/ɔ:/","/θ/","/ð/",
            "/ʃ/","/tʃ/","/ŋ/","/ə/","/aɪ/","/eɪ/","/əʊ/","/aʊ/",
            "/ɪə/","/eə/","/p/","/b/","/t/","/d/","/k/","/g/"
        };

        const double W = width(), H = height();
        const double scale = std::min(W, H) * 0.25 * zoom_;
        const double cx = W * 0.5, cy = H * 0.52;
        const int n = mode_order_;

        auto proj = [&](double x, double y, double z, double& oz) -> QPointF {
            double x1 = x * std::cos(yaw_) + z * std::sin(yaw_);
            double z1 = -x * std::sin(yaw_) + z * std::cos(yaw_);
            double y2 = y * std::cos(pitch_) - z1 * std::sin(pitch_);
            double z2 = y * std::sin(pitch_) + z1 * std::cos(pitch_);
            double persp = 4.6 / (4.6 - z2);
            oz = z2;
            return QPointF(cx + x1 * scale * persp, cy - y2 * scale * persp);
        };

        struct FaceSpec { int axis; int dir; QColor col; };
        const std::vector<FaceSpec> faces = {
            { 2,  1, QColor("#2563EB") }, // +Z front blue
            { 2, -1, QColor("#059669") }, // -Z back green
            { 0,  1, QColor("#DC2626") }, // +X right red
            { 0, -1, QColor("#EA580C") }, // -X left orange
            { 1,  1, QColor("#F59E0B") }, // +Y top yellow
            { 1, -1, QColor("#7C3AED") }  // -Y bottom purple
        };

        int labelIdx = 0;
        const double half = 1.0, step = (half * 2.0) / n, pad = 0.04;
        for (int f = 0; f < 6; ++f) {
            const auto& fs = faces[f];
            // Outward normal test in rotated space
            double nx = (fs.axis == 0 ? fs.dir : 0), ny = (fs.axis == 1 ? fs.dir : 0), nz = (fs.axis == 2 ? fs.dir : 0);
            double rnz1 = -nx * std::sin(yaw_) + nz * std::cos(yaw_);
            double rnz2 = ny * std::sin(pitch_) + rnz1 * std::cos(pitch_);
            if (rnz2 <= 0.05) { labelIdx += n * n; continue; }

            for (int u = 0; u < n; ++u) {
                for (int v = 0; v < n; ++v) {
                    double u0 = -half + u * step + pad, u1 = -half + (u + 1) * step - pad;
                    double v0 = -half + v * step + pad, v1 = -half + (v + 1) * step - pad;
                    QPointF pts[4];
                    double zsum = 0.0;
                    for (int c = 0; c < 4; ++c) {
                        double pu = (c == 0 || c == 3) ? u0 : u1;
                        double pv = (c < 2) ? v0 : v1;
                        double x = 0, y = 0, z = 0;
                        if (fs.axis == 0) { x = fs.dir * half; y = pv; z = pu; }
                        else if (fs.axis == 1) { y = fs.dir * half; x = pu; z = pv; }
                        else { z = fs.dir * half; x = pu; y = pv; }
                        double oz = 0;
                        pts[c] = proj(x, y, z, oz);
                        zsum += oz;
                    }
                    FaceTile tile;
                    tile.depth = zsum * 0.25;
                    tile.poly << pts[0] << pts[1] << pts[2] << pts[3];
                    tile.center = (pts[0] + pts[1] + pts[2] + pts[3]) * 0.25;
                    tile.color = fs.col;
                    if (mode_order_ == 3) tile.label = letters[labelIdx % letters.size()];
                    else tile.label = ipas[labelIdx % ipas.size()];
                    tiles.push_back(tile);
                    ++labelIdx;
                }
            }
        }

        std::sort(tiles.begin(), tiles.end(), [](const FaceTile& a, const FaceTile& b) {
            return a.depth < b.depth;
        });

        hit_tiles_.clear();
        QFont fnt = font();
        fnt.setBold(true);
        fnt.setPointSize(mode_order_ == 3 ? 10 : 8);
        p.setFont(fnt);

        for (const auto& t : tiles) {
            bool sel = (!selected_label_.isEmpty() && t.label.compare(selected_label_, Qt::CaseInsensitive) == 0);
            QColor fill = t.color;
            fill.setAlpha(sel ? 245 : 210);
            p.setBrush(fill);
            p.setPen(QPen(sel ? QColor("#FFFFFF") : QColor("#0F172A"), sel ? 2.4 : 1.1));
            p.drawPolygon(t.poly);

            p.setPen(QColor("#FFFFFF"));
            QRectF tr(t.center.x() - 22, t.center.y() - 12, 44, 24);
            p.drawText(tr, Qt::AlignCenter, t.label);
            hit_tiles_.push_back({t.poly, t.label});
        }

        // Top-left hint pill
        p.setPen(QColor(pal_.text_muted));
        QFont hf = font(); hf.setPointSize(9); p.setFont(hf);
        p.drawText(QRectF(12, 8, 220, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   mode_order_ == 3 ? QStringLiteral("3×3 键盘字母魔方 · 拖拽旋转/点击查词")
                                    : QStringLiteral("2×2 音标立方 · 点击筛选音标"));
    }

    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = true;
            moved_ = false;
            last_pos_ = e->pos();
            press_pos_ = e->pos();
            setCursor(Qt::ClosedHandCursor);
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        if (dragging_) {
            QPoint d = e->pos() - last_pos_;
            if ((e->pos() - press_pos_).manhattanLength() > 4) moved_ = true;
            yaw_ += d.x() * 0.012;
            pitch_ = std::max(-1.25, std::min(1.25, pitch_ + d.y() * 0.012));
            last_pos_ = e->pos();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* e) override {
        if (e->button() == Qt::LeftButton) {
            dragging_ = false;
            setCursor(Qt::OpenHandCursor);
            if (!moved_) {
                for (int i = static_cast<int>(hit_tiles_.size()) - 1; i >= 0; --i) {
                    if (hit_tiles_[i].first.containsPoint(e->pos(), Qt::OddEvenFill)) {
                        QString lbl = hit_tiles_[i].second;
                        if (lbl != QStringLiteral("*")) {
                            selected_label_ = lbl;
                            update();
                            if (onCellSelected) onCellSelected(lbl, mode_order_ == 2);
                        }
                        break;
                    }
                }
            }
        }
    }

    void wheelEvent(QWheelEvent* e) override {
        zoom_ = std::max(0.65, std::min(1.5, zoom_ + (e->angleDelta().y() > 0 ? 0.07 : -0.07)));
        update();
    }

private:
    ThemePalette pal_ = get_theme_palette("white");
    int mode_order_ = 3;
    double yaw_ = -0.58;
    double pitch_ = 0.44;
    double zoom_ = 1.0;
    bool auto_rotate_ = false;
    bool dragging_ = false;
    bool moved_ = false;
    QPoint last_pos_, press_pos_;
    QString selected_label_;
    QTimer* timer_ = nullptr;
    std::vector<std::pair<QPolygonF, QString>> hit_tiles_;
};

// ============================================================================
// 4. Wi-Fi Concentric Arc Vault Explorer Widget (4 Layers: Root + Rings 1..3)
// ============================================================================
class VaultArcWidget : public QWidget {
public:
    struct RingItem {
        std::string name;
        std::string rel_path;
        bool is_dir = false;
        std::vector<RingItem> children;
    };

    std::function<void(const std::string& relPath, bool isDir)> onItemClicked;
    std::function<void(const std::string& relPath, bool isDir, const QPoint& globalPos)> onItemRightClicked;

    explicit VaultArcWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(240);
        setMouseTracking(true);
    }

    void setPaletteTheme(const ThemePalette& pal) { pal_ = pal; update(); }

    void setTreeData(const JsonValue& tree) {
        root_name_ = tree.get("root_name").str_val;
        if (root_name_.empty()) root_name_ = "Vault";
        root_exists_ = tree.get("root_exists").bool_val;
        ring1_.clear();
        const auto& l1 = tree.get("items");
        if (l1.is_array()) {
            for (const auto& j : l1.arr_val) ring1_.push_back(parseNode(j));
        }
        rebuildRings();
        update();
    }

    void setActivePath(const std::string& relPath) { active_path_ = relPath; update(); }
    void setExpandedDir(int ring, const std::string& relPath) {
        if (ring == 1) { sel_r1_dir_ = (sel_r1_dir_ == relPath ? "" : relPath); sel_r2_dir_.clear(); scroll_r2_ = 0; scroll_r3_ = 0; }
        else if (ring == 2) { sel_r2_dir_ = (sel_r2_dir_ == relPath ? "" : relPath); scroll_r3_ = 0; }
        rebuildRings();
        update();
    }
    void ensureDirExpanded(const std::string& relPath) {
        for (const auto& it : ring1_) {
            if (it.is_dir && it.rel_path == relPath) { sel_r1_dir_ = relPath; break; }
            for (const auto& c : it.children) {
                if (c.is_dir && c.rel_path == relPath) { sel_r1_dir_ = it.rel_path; sel_r2_dir_ = c.rel_path; break; }
            }
        }
        rebuildRings();
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF r = rect().adjusted(2, 2, -2, -2);
        QPainterPath bg;
        bg.addRoundedRect(r, 12, 12);
        p.fillPath(bg, QColor(pal_.bg_app));
        p.setPen(QPen(QColor(pal_.border_line), 1));
        p.drawPath(bg);

        hits_.clear();
        const double W = width(), H = height();
        const double rootX = std::max(44.0, W * 0.11);
        const double rootY = H * 0.5;
        const double span = std::max(120.0, W - rootX - 95.0);
        const double radii[3] = { span * 0.30, span * 0.62, span * 0.96 };
        const int caps[3] = { 5, 7, 9 };

        // Dashed concentric arc tracks
        for (int k = 0; k < 3; ++k) {
            QRectF arcRect(rootX - radii[k], rootY - radii[k], radii[k] * 2.0, radii[k] * 2.0);
            p.setPen(QPen(QColor(pal_.border_line), 1.4, Qt::DashLine));
            p.drawArc(arcRect, -58 * 16, 116 * 16);
        }

        // Root node dot
        p.setBrush(QColor(pal_.blue_soft));
        p.setPen(QPen(QColor(pal_.blue), 2));
        p.drawEllipse(QPointF(rootX, rootY), 14, 14);
        p.setBrush(QColor(pal_.bg_active));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPointF(rootX, rootY), 6.5, 6.5);

        QFont f = font(); f.setBold(true); f.setPointSize(9); p.setFont(f);
        p.setPen(QColor(pal_.text_primary));
        p.drawText(QRectF(rootX - 38, rootY + 17, 76, 18), Qt::AlignCenter,
                   QString::fromStdString(root_name_).left(10));

        if (ring1_.empty()) {
            p.setPen(QColor(pal_.text_muted));
            QFont ef = font(); ef.setPointSize(9.5); p.setFont(ef);
            p.drawText(QRectF(rootX + 30, rootY - 26, W - rootX - 45, 52),
                       Qt::AlignCenter | Qt::TextWordWrap,
                       root_exists_ ? QStringLiteral("当前目录为空，请点击上方「+ 目录」或「+ 笔记」")
                                    : QStringLiteral("尚未选择知识库文件夹，请点击左上「选择」"));
            return;
        }

        const std::vector<RingItem>* lists[3] = { &ring1_, &ring2_, &ring3_ };
        int* scrolls[3] = { &scroll_r1_, &scroll_r2_, &scroll_r3_ };
        for (int k = 0; k < 3; ++k) {
            const auto& vec = *lists[k];
            if (vec.empty()) continue;
            int cap = caps[k];
            int maxScr = std::max(0, static_cast<int>(vec.size()) - cap);
            *scrolls[k] = std::max(0, std::min(maxScr, *scrolls[k]));
            int vis = std::min(cap, static_cast<int>(vec.size()));
            double startDeg = -48.0, endDeg = 48.0;
            for (int i = 0; i < vis; ++i) {
                const auto& item = vec[*scrolls[k] + i];
                double deg = (vis == 1) ? 0.0 : (startDeg + (endDeg - startDeg) * i / (vis - 1));
                double rad = deg * 3.14159265 / 180.0;
                double nx = rootX + radii[k] * std::cos(rad);
                double ny = rootY + radii[k] * std::sin(rad);

                bool isAct = (item.rel_path == active_path_) ||
                             (item.rel_path == sel_r1_dir_) || (item.rel_path == sel_r2_dir_);
                bool isHov = (item.rel_path == hover_path_);

                if (item.is_dir) {
                    p.setBrush(QColor(isAct ? pal_.blue_soft : pal_.amber_soft));
                    p.setPen(QPen(QColor(isAct ? pal_.blue : pal_.amber), isAct || isHov ? 2.2 : 1.5));
                    p.drawEllipse(QPointF(nx, ny), 9.0, 9.0);
                    // Miniature folder glyph inside node
                    p.setBrush(QColor(isAct ? pal_.blue : pal_.amber));
                    p.setPen(Qt::NoPen);
                    p.drawRoundedRect(QRectF(nx - 4.0, ny - 2.5, 8.0, 5.5), 1.2, 1.2);
                    p.drawRect(QRectF(nx - 4.0, ny - 4.0, 4.0, 2.0));
                } else {
                    p.setBrush(QColor(isAct ? pal_.blue : pal_.bg_card));
                    p.setPen(QPen(QColor(isAct ? pal_.blue : pal_.text_muted), isAct || isHov ? 2.2 : 1.5));
                    p.drawEllipse(QPointF(nx, ny), 7.5, 7.5);
                    p.setBrush(QColor(isAct ? "#FFFFFF" : pal_.text_muted));
                    p.setPen(Qt::NoPen);
                    p.drawEllipse(QPointF(nx, ny), 2.3, 2.3);
                }

                QString nm = QString::fromStdString(item.name);
                if (nm.endsWith(".md")) nm.chop(3);
                if (nm.size() > 11) nm = nm.left(10) + "…";
                QFont lf = font(); lf.setPointSize(8); lf.setBold(isAct); p.setFont(lf);
                QFontMetrics fm(lf);
                int tw = std::min(96, fm.horizontalAdvance(nm) + 12);
                QRectF capRect(nx + 11, ny - 10, tw, 20);
                p.setBrush(QColor(isAct ? pal_.blue_soft : (isHov ? pal_.bg_subtle : pal_.bg_card)));
                p.setPen(QPen(QColor(isAct ? pal_.blue : pal_.border_line), isAct || isHov ? 1.2 : 0.9));
                p.drawRoundedRect(capRect, 6, 6);
                p.setPen(QColor(isAct ? pal_.blue : pal_.text_primary));
                p.drawText(capRect.adjusted(6, 0, -4, 0), Qt::AlignLeft | Qt::AlignVCenter, nm);

                hits_.push_back({QRectF(nx - 12, ny - 12, tw + 26, 24), item.rel_path, item.is_dir, k + 1});
            }
        }
    }

    void mouseMoveEvent(QMouseEvent* e) override {
        std::string found;
        for (const auto& h : hits_) {
            if (h.box.contains(e->pos())) { found = h.rel_path; break; }
        }
        if (found != hover_path_) {
            hover_path_ = found;
            setCursor(found.empty() ? Qt::ArrowCursor : Qt::PointingHandCursor);
            update();
        }
    }

    void mousePressEvent(QMouseEvent* e) override {
        for (const auto& h : hits_) {
            if (h.box.contains(e->pos())) {
                if (e->button() == Qt::LeftButton) {
                    if (h.is_dir) setExpandedDir(h.ring, h.rel_path);
                    if (onItemClicked) onItemClicked(h.rel_path, h.is_dir);
                } else if (e->button() == Qt::RightButton && onItemRightClicked) {
                    onItemRightClicked(h.rel_path, h.is_dir, e->globalPosition().toPoint());
                }
                return;
            }
        }
    }

    void wheelEvent(QWheelEvent* e) override {
        int dir = e->angleDelta().y() > 0 ? -1 : 1;
        double x = e->position().x();
        double rootX = std::max(44.0, width() * 0.11);
        double span = std::max(120.0, width() - rootX - 95.0);
        if (x < rootX + span * 0.46) scroll_r1_ += dir;
        else if (x < rootX + span * 0.78) scroll_r2_ += dir;
        else scroll_r3_ += dir;
        update();
    }

private:
    struct Hit { QRectF box; std::string rel_path; bool is_dir; int ring; };

    RingItem parseNode(const JsonValue& j) {
        RingItem it;
        it.name = j.get("name").str_val;
        it.rel_path = j.get("rel_path").str_val;
        it.is_dir = j.get("is_dir").bool_val;
        const auto& ch = j.get("children");
        if (ch.is_array()) {
            for (const auto& c : ch.arr_val) it.children.push_back(parseNode(c));
        }
        return it;
    }

    void rebuildRings() {
        ring2_.clear(); ring3_.clear();
        for (const auto& it : ring1_) {
            if (it.is_dir && it.rel_path == sel_r1_dir_) { ring2_ = it.children; break; }
        }
        for (const auto& it : ring2_) {
            if (it.is_dir && it.rel_path == sel_r2_dir_) { ring3_ = it.children; break; }
        }
    }

    ThemePalette pal_ = get_theme_palette("white");
    std::string root_name_ = "Vault";
    bool root_exists_ = false;
    std::string active_path_, hover_path_, sel_r1_dir_, sel_r2_dir_;
    std::vector<RingItem> ring1_, ring2_, ring3_;
    int scroll_r1_ = 0, scroll_r2_ = 0, scroll_r3_ = 0;
    std::vector<Hit> hits_;
};

// ============================================================================
// 5. Native Algorithm & Data-Structure Interactive Canvas (DsVisualizerWidget)
// ============================================================================
class DsVisualizerWidget : public QWidget {
public:
    explicit DsVisualizerWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setMinimumHeight(146);
    }

    void setPaletteTheme(const ThemePalette& pal) { pal_ = pal; update(); }
    void setLesson(const QString& id, const QString& vizType) {
        lesson_id_ = id;
        viz_type_ = vizType;
        step_ = 0;
        caption_ = computeCaption();
        update();
    }
    void setStep(int s) {
        step_ = ((s % 4) + 4) % 4;
        caption_ = computeCaption();
        update();
    }
    int step() const { return step_; }

    QString captionText() const { return caption_.isEmpty() ? computeCaption() : caption_; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        QRectF r = rect().adjusted(1, 1, -1, -1);
        QPainterPath bg;
        bg.addRoundedRect(r, 11, 11);
        p.fillPath(bg, QColor(pal_.is_dark ? "#131924" : "#FAFBFD"));
        p.setPen(QPen(QColor(pal_.border_line), 1));
        p.drawPath(bg);

        // Subtle engineering dot grid background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(pal_.is_dark ? "#222C3D" : "#E2E8F0"));
        for (int gx = 16; gx < width() - 10; gx += 20) {
            for (int gy = 14; gy < height() - 10; gy += 20) {
                p.drawEllipse(QPointF(gx, gy), 1.0, 1.0);
            }
        }

        // Uniform aspect-preserving scaling centered inside the canvas
        const double sceneW = 510.0, sceneH = 148.0;
        double s = std::min((width() - 12.0) / sceneW, (height() - 12.0) / sceneH);
        s = std::max(0.45, s);
        double ox = (width() - sceneW * s) * 0.5;
        double oy = (height() - sceneH * s) * 0.5;

        p.save();
        p.translate(ox, oy);
        p.scale(s, s);
        paintScene(p);
        p.restore();
    }

private:
    void drawArrow(QPainter& p, QPointF a, QPointF b, const QColor& col, double width = 1.8) {
        p.setPen(QPen(col, width));
        p.drawLine(a, b);
        double ang = std::atan2(b.y() - a.y(), b.x() - a.x());
        QPointF p1 = b - QPointF(std::cos(ang - 0.45) * 7, std::sin(ang - 0.45) * 7);
        QPointF p2 = b - QPointF(std::cos(ang + 0.45) * 7, std::sin(ang + 0.45) * 7);
        QPolygonF head; head << b << p1 << p2;
        p.setBrush(col);
        p.drawPolygon(head);
    }

    void paintScene(QPainter& p) {
        const QColor blue("#2563EB"), green("#059669"), red("#DC2626"), slate("#64748B"), amber("#D97706");
        const QColor fillBlue("#EFF6FF"), fillGreen("#ECFDF5"), fillCard("#FFFFFF"), fillRed("#FEF2F2");

        QFont f(QStringLiteral("Consolas")); f.setBold(true); f.setPointSize(10);
        p.setFont(f);

        if (lesson_id_ == "5.6") {
            const int aD[4] = {8, 6, 5, 0}, bD[4] = {7, 8, 4, 0}, car[4] = {0, 1, 1, 1}, sums[4] = {5, 5, 0, 1};
            p.setPen(slate);
            p.drawText(30, 40, "a.d[i]"); p.drawText(30, 70, "b.d[i]");
            p.setPen(green); p.drawText(30, 108, "c.d[i]");
            for (int i = 0; i < 4; ++i) {
                double x = 115 + i * 68;
                bool done = i <= step_, cur = i == step_;
                p.setBrush(QColor("#F8FAFC")); p.setPen(QPen(QColor("#CBD5E1"), 1.2));
                p.drawRoundedRect(x, 22, 46, 24, 5, 5);
                p.drawRoundedRect(x, 52, 46, 24, 5, 5);
                p.setPen(QColor("#111827"));
                p.drawText(QRectF(x, 22, 46, 24), Qt::AlignCenter, i < 3 ? QString::number(aD[i]) : "-");
                p.drawText(QRectF(x, 52, 46, 24), Qt::AlignCenter, i < 3 ? QString::number(bD[i]) : "-");

                p.setBrush(cur ? fillBlue : (done ? fillGreen : fillCard));
                p.setPen(QPen(cur ? blue : (done ? green : QColor("#CBD5E1")), cur ? 2.2 : 1.4));
                p.drawRoundedRect(x, 90, 46, 26, 6, 6);
                p.setPen(done ? QColor("#111827") : QColor("#94A3B8"));
                p.drawText(QRectF(x, 90, 46, 26), Qt::AlignCenter, done ? QString::number(sums[i]) : "?");
                p.setPen(slate);
                p.drawText(QRectF(x, 120, 46, 16), Qt::AlignCenter, QString("d[%1]").arg(i));
            }
            p.setPen(amber); p.drawText(395, 68, QString("carry = %1").arg(car[step_]));
            p.setPen(green); p.drawText(395, 106, QStringLiteral("输出: 1055"));
            caption_ = QString("568 + 487 竖式第 %1 位相加").arg(step_);
        } else if (lesson_id_ == "2.7" || lesson_id_ == "2.8") {
            const int vals[4] = {10, 10, 25, 25};
            p.setBrush(step_ >= 2 ? fillGreen : fillBlue); p.setPen(QPen(blue, 2));
            p.drawRoundedRect(46, 32, 156, 72, 10, 10);
            p.setPen(blue);
            p.drawText(QRectF(46, 38, 156, 20), Qt::AlignCenter, lesson_id_ == "2.8" ? "Stu s (0x7ff0)" : "int a (0x7ff0)");
            p.setPen(QColor("#111827"));
            p.drawText(QRectF(46, 62, 156, 30), Qt::AlignCenter, lesson_id_ == "2.8" ? QString("score=%1").arg(vals[step_]) : QString("a = %1").arg(vals[step_]));

            bool linked = step_ >= 1;
            p.setBrush(fillCard); p.setPen(QPen(linked ? green : slate, 2));
            p.drawRoundedRect(298, 32, 165, 72, 10, 10);
            p.setPen(green);
            p.drawText(QRectF(298, 38, 165, 20), Qt::AlignCenter, lesson_id_ == "2.8" ? "Stu* p = &s" : "int* p = &a");
            p.setPen(QColor("#111827"));
            p.drawText(QRectF(298, 62, 165, 30), Qt::AlignCenter, linked ? "0x7ff0" : "nullptr");
            if (linked) drawArrow(p, QPointF(296, 68), QPointF(206, 68), green, 2.2);
            caption_ = step_ == 0 ? "栈上分配变量内存" : (step_ == 1 ? "指针 p 记录目标内存地址" : "通过 *p / p->score 修改原数据");
        } else if (lesson_id_ == "2.5B" || lesson_id_ == "3.6" || lesson_id_ == "6.3") {
            const QStringList cs = {"P","A","T","I","T","A","P","\\0"};
            int iPtr = std::min(3, step_), jPtr = 6 - iPtr;
            for (int i = 0; i < 8; ++i) {
                double x = 35 + i * 56;
                bool nul = (i == 7), hit = (i == iPtr || i == jPtr);
                p.setBrush(nul ? fillRed : (hit ? fillBlue : fillCard));
                p.setPen(QPen(nul ? red : (hit ? blue : slate), hit || nul ? 2.0 : 1.3));
                p.drawRoundedRect(x, 44, 46, 44, 7, 7);
                p.setPen(nul ? red : QColor("#111827"));
                p.drawText(QRectF(x, 44, 46, 44), Qt::AlignCenter, cs[i]);
                p.setPen(slate);
                p.drawText(QRectF(x, 92, 46, 16), Qt::AlignCenter, QString("[%1]").arg(i));
                if (i == iPtr) { p.setPen(blue); p.drawText(QRectF(x, 20, 46, 18), Qt::AlignCenter, "L"); }
                if (i == jPtr && jPtr != iPtr) { p.setPen(green); p.drawText(QRectF(x, 20, 46, 18), Qt::AlignCenter, "R"); }
            }
            caption_ = QString("C 串以 '\\0' 结尾 · 指针 L=%1, R=%2").arg(iPtr).arg(jPtr);
        } else if (lesson_id_ == "2.5A" || lesson_id_ == "4.1" || lesson_id_ == "4.6") {
            const int frames[4][5] = {{5,1,4,2,8},{1,4,2,5,8},{1,2,4,5,8},{1,2,4,5,8}};
            for (int i = 0; i < 5; ++i) {
                int v = frames[step_][i];
                double x = 70 + i * 76, h = v * 9 + 14;
                bool ok = i >= std::max(0, 4 - step_);
                p.setBrush(ok ? fillGreen : fillBlue);
                p.setPen(QPen(ok ? green : blue, 1.8));
                p.drawRoundedRect(x, 108 - h, 50, h, 6, 6);
                p.setPen(QColor("#111827"));
                p.drawText(QRectF(x, 86 - h, 50, 20), Qt::AlignCenter, QString::number(v));
                p.setPen(slate);
                p.drawText(QRectF(x, 114, 50, 16), Qt::AlignCenter, QString("a[%1]").arg(i));
            }
            caption_ = QString("第 %1 轮排序有序区扩大").arg(step_ + 1);
        } else if (lesson_id_ == "5.4") {
            for (int i = 0; i < 9; ++i) {
                int v = i + 2;
                double x = 32 + i * 50;
                bool c2 = step_ >= 1 && v > 2 && v % 2 == 0;
                bool c3 = step_ >= 2 && v > 3 && v % 3 == 0;
                bool crs = c2 || c3;
                bool prm = (step_ == 3 && !crs);
                p.setBrush(crs ? fillRed : (prm ? fillGreen : fillBlue));
                p.setPen(QPen(crs ? red : (prm ? green : blue), 1.6));
                p.drawRoundedRect(x, 46, 42, 42, 7, 7);
                p.setPen(crs ? QColor("#9CA3AF") : QColor("#111827"));
                p.drawText(QRectF(x, 46, 42, 42), Qt::AlignCenter, QString::number(v));
                if (crs) { p.setPen(QPen(red, 2)); p.drawLine(x + 6, 82, x + 36, 52); }
            }
            caption_ = step_ == 0 ? "初始化待筛区间 2..10" : (step_ == 1 ? "2 筛掉偶数" : (step_ == 2 ? "3 筛掉 9" : "留下绿色素数 2,3,5,7"));
        } else if (lesson_id_ == "5.1" || lesson_id_ == "5.3" || lesson_id_ == "5.7") {
            const char* ps[4] = {"(48, 18)", "(18, 12)", "(12, 6)", "gcd = 6"};
            for (int i = 0; i < 4; ++i) {
                double x = 22 + i * 122;
                bool cur = (i == step_), done = (i < step_);
                p.setBrush(cur ? fillBlue : (done ? fillGreen : fillCard));
                p.setPen(QPen(cur ? blue : (done ? green : slate), cur ? 2.2 : 1.4));
                p.drawRoundedRect(x, 44, 96, 48, 8, 8);
                p.setPen(QColor("#111827"));
                p.drawText(QRectF(x, 44, 96, 48), Qt::AlignCenter, ps[i]);
                if (i > 0) drawArrow(p, QPointF(x - 24, 68), QPointF(x - 4, 68), blue);
            }
            caption_ = QString("欧几里得 gcd(a, b) = gcd(b, a %% b) · Step %1").arg(step_ + 1);
        } else if (lesson_id_ == "0.3" || lesson_id_ == "3.3" || lesson_id_ == "8.2") {
            const int d[12] = {0,1,2,1, 1,9,3,2, 2,3,4,3};
            for (int r = 0; r < 3; ++r) {
                for (int c = 0; c < 4; ++c) {
                    double x = 148 + c * 54, y = 16 + r * 38;
                    int v = d[r * 4 + c];
                    bool wall = (v == 9), vis = !wall && v <= step_;
                    p.setBrush(wall ? QColor("#334155") : (vis ? fillBlue : fillCard));
                    p.setPen(QPen(vis ? blue : QColor("#CBD5E1"), 1.6));
                    p.drawRoundedRect(x, y, 46, 32, 6, 6);
                    p.setPen(wall ? QColor("#FFFFFF") : (vis ? blue : slate));
                    p.drawText(QRectF(x, y, 46, 32), Qt::AlignCenter, wall ? "#" : (vis ? QString("d=%1").arg(v) : "."));
                }
            }
            caption_ = QString("网格 BFS / 模拟扫描当前层级 d = %1").arg(step_);
        } else if (viz_type_ == "linked_list") {
            p.setPen(slate); p.drawText(18, 72, "NULL");
            for (int i = 0; i < 4; ++i) {
                double x = 86 + i * 102;
                bool done = i < step_, cur = i == step_;
                p.setBrush(done ? fillGreen : (cur ? fillBlue : fillCard));
                p.setPen(QPen(done ? green : (cur ? blue : slate), cur ? 2.2 : 1.5));
                p.drawRoundedRect(x, 52, 64, 34, 7, 7);
                p.drawLine(x + 42, 52, x + 42, 86);
                p.setPen(QColor("#111827"));
                p.drawText(QRectF(x, 52, 42, 34), Qt::AlignCenter, lesson_id_ == "7.3B" ? QString("a%1").arg(i+1) : QString::number(i+1));
                if (i > 0) drawArrow(p, QPointF(x - 36, 69), QPointF(x - 4, 69), done || cur ? blue : slate);
                if (cur) { p.setPen(blue); p.drawText(QRectF(x, 22, 64, 18), Qt::AlignCenter, "curr"); }
            }
            caption_ = lesson_id_ == "7.3B" ? "静态链表 Node[addr] 数组排序重组" : QString("三指针原地反转进度 %1/4").arg(step_ + 1);
        } else if (viz_type_ == "tree" || viz_type_ == "bst") {
            const QPointF pts[6] = {{255,26},{165,72},{345,72},{115,118},{215,118},{395,118}};
            const int edges[5][2] = {{0,1},{0,2},{1,3},{1,4},{2,5}};
            const int order[4] = {0,1,3,2};
            const char* lbls[6] = {"70","61","88","12","65","96"};
            p.setPen(QPen(QColor("#CBD5E1"), 2));
            for (auto& e : edges) p.drawLine(pts[e[0]], pts[e[1]]);
            for (int i = 0; i < 6; ++i) {
                bool act = (i == order[step_]);
                p.setBrush(act ? blue : fillCard);
                p.setPen(QPen(act ? blue : slate, 2));
                p.drawEllipse(pts[i], 16, 16);
                p.setPen(act ? QColor("#FFFFFF") : QColor("#111827"));
                p.drawText(QRectF(pts[i].x()-16, pts[i].y()-16, 32, 32), Qt::AlignCenter, lbls[i]);
            }
            caption_ = QString("二叉树 / BST / 堆遍历节点 %1").arg(lbls[order[step_]]);
        } else if (viz_type_ == "graph") {
            const QPointF gp[4] = {{95,68},{235,30},{235,108},{395,68}};
            const int ge[5][3] = {{0,1,2},{0,2,5},{1,2,1},{1,3,6},{2,3,2}};
            const char* ds[4][4] = {{"0","0","0","0"},{"∞","2","2","2"},{"∞","5","3","3"},{"∞","∞","8","5"}};
            for (auto& e : ge) {
                bool act = (step_ == 1 && e[0] == 0) || (step_ == 2 && e[0] == 1) || (step_ == 3 && e[0] == 2);
                drawArrow(p, gp[e[0]], gp[e[1]], act ? blue : QColor("#CBD5E1"), act ? 2.2 : 1.5);
            }
            for (int i = 0; i < 4; ++i) {
                bool done = i <= step_;
                p.setBrush(done ? fillBlue : fillCard);
                p.setPen(QPen(done ? blue : slate, 2));
                p.drawEllipse(gp[i], 18, 18);
                p.setPen(QColor("#111827"));
                p.drawText(QRectF(gp[i].x()-18, gp[i].y()-18, 36, 36), Qt::AlignCenter, QString::number(i + 1));
                p.setPen(green);
                p.drawText(QRectF(gp[i].x()-26, gp[i].y()+18, 52, 16), Qt::AlignCenter, QString("d=%1").arg(ds[i][step_]));
            }
            caption_ = QString("图算法松弛第 %1 轮").arg(step_ + 1);
        } else if (viz_type_ == "dsu") {
            const int comps = step_ < 2 ? 3 : (step_ == 2 ? 2 : 1);
            const char* txts[3] = {"{1,2}  {3}  {4,5}", "{1,2,3}  {4,5}", "{1,2,3,4,5}"};
            int idx = step_ < 2 ? 0 : (step_ == 2 ? 1 : 2);
            p.setBrush(fillBlue); p.setPen(QPen(blue, 2, Qt::DashLine));
            p.drawRoundedRect(65, 34, 380, 68, 12, 12);
            p.setPen(blue);
            p.drawText(QRectF(65, 34, 380, 68), Qt::AlignCenter, QString(txts[idx]));
            caption_ = QString("并查集合并连通块：当前 comps = %1").arg(comps);
        } else if (viz_type_ == "dp" || viz_type_ == "kmp" || viz_type_ == "bit" || viz_type_ == "bsearch" || viz_type_ == "two_ptr" || viz_type_ == "queue" || viz_type_ == "stack") {
            const int arr[6] = {2, 5, 8, 12, 16, 23};
            for (int i = 0; i < 6; ++i) {
                double x = 58 + i * 66;
                bool focus = (i == step_ + 1);
                p.setBrush(focus ? fillBlue : (i <= step_ ? fillGreen : fillCard));
                p.setPen(QPen(focus ? blue : (i <= step_ ? green : QColor("#CBD5E1")), focus ? 2.2 : 1.5));
                p.drawRoundedRect(x, 44, 52, 42, 8, 8);
                p.setPen(QColor("#111827"));
                p.drawText(QRectF(x, 44, 52, 42), Qt::AlignCenter, QString::number(arr[i]));
                p.setPen(slate);
                p.drawText(QRectF(x, 92, 52, 18), Qt::AlignCenter, QString("[%1]").arg(i));
            }
            caption_ = viz_type_ == "dp" ? QString("状态递推更新 dp[%1]").arg(step_ + 1)
                   : (viz_type_ == "kmp" ? QString("KMP 前缀数组 pi[] 失配回退")
                   : (viz_type_ == "bit" ? QString("树状数组 lowbit(x) / 散列映射")
                   : (viz_type_ == "stack" ? QString("LIFO 栈指针 top = %1").arg(step_)
                   : QString("活跃指针聚焦位置 [%1]").arg(step_ + 1))));
        } else {
            const char* lbls[4] = {"O(log n)", "O(n)", "O(n log n)", "O(n^2)"};
            const int ws[4] = {75, 145, 225, 315};
            for (int i = 0; i < 4; ++i) {
                double y = 14 + i * 30;
                bool act = (i == step_);
                p.setPen(act ? blue : slate);
                p.drawText(16, static_cast<int>(y + 18), lbls[i]);
                p.setBrush(act ? blue : QColor("#94A3B8"));
                p.setPen(Qt::NoPen);
                p.drawRoundedRect(118, y + 4, ws[i], 18, 5, 5);
            }
            caption_ = QString("时间复杂度对比 · 1s ≈ 10^8 次运算");
        }
    }

    QString computeCaption() const {
        if (lesson_id_ == "5.6") return QString("568 + 487 竖式第 %1 位相加").arg(step_);
        if (lesson_id_ == "2.7" || lesson_id_ == "2.8") {
            return step_ == 0 ? QStringLiteral("栈上分配变量内存")
                 : (step_ == 1 ? QStringLiteral("指针 p 记录目标内存地址")
                               : QStringLiteral("通过 *p / p->score 修改原数据"));
        }
        if (lesson_id_ == "2.5B" || lesson_id_ == "3.6" || lesson_id_ == "6.3") {
            int iPtr = std::min(3, step_), jPtr = 6 - iPtr;
            return QString("C 串以 '\\0' 结尾 · 指针 L=%1, R=%2").arg(iPtr).arg(jPtr);
        }
        if (lesson_id_ == "2.5A" || lesson_id_ == "4.1" || lesson_id_ == "4.6") {
            return QString("第 %1 轮排序有序区扩大").arg(step_ + 1);
        }
        if (lesson_id_ == "5.4") {
            return step_ == 0 ? QStringLiteral("初始化待筛区间 2..10")
                 : (step_ == 1 ? QStringLiteral("2 筛掉偶数")
                 : (step_ == 2 ? QStringLiteral("3 筛掉 9")
                               : QStringLiteral("留下绿色素数 2,3,5,7")));
        }
        if (lesson_id_ == "5.1" || lesson_id_ == "5.3" || lesson_id_ == "5.7") {
            return QString("欧几里得 gcd(a, b) = gcd(b, a %% b) · Step %1").arg(step_ + 1);
        }
        if (lesson_id_ == "0.3" || lesson_id_ == "3.3" || lesson_id_ == "8.2") {
            return QString("网格 BFS / 模拟扫描当前层级 d = %1").arg(step_);
        }
        if (viz_type_ == "linked_list") {
            return lesson_id_ == "7.3B" ? QStringLiteral("静态链表 Node[addr] 数组排序重组")
                                        : QString("三指针原地反转进度 %1/4").arg(step_ + 1);
        }
        if (viz_type_ == "tree" || viz_type_ == "bst") {
            const char* lbls[4] = {"70", "61", "12", "88"};
            return QString("二叉树 / BST / 堆遍历节点 %1").arg(lbls[step_]);
        }
        if (viz_type_ == "graph") return QString("图算法松弛第 %1 轮").arg(step_ + 1);
        if (viz_type_ == "dsu") {
            int comps = step_ < 2 ? 3 : (step_ == 2 ? 2 : 1);
            return QString("并查集合并连通块：当前 comps = %1").arg(comps);
        }
        if (viz_type_ == "dp" || viz_type_ == "kmp" || viz_type_ == "bit" ||
            viz_type_ == "bsearch" || viz_type_ == "two_ptr" ||
            viz_type_ == "queue" || viz_type_ == "stack") {
            return viz_type_ == "dp" ? QString("状态递推更新 dp[%1]").arg(step_ + 1)
                 : (viz_type_ == "kmp" ? QStringLiteral("KMP 前缀数组 pi[] 失配回退")
                 : (viz_type_ == "bit" ? QStringLiteral("树状数组 lowbit(x) / 散列映射")
                 : (viz_type_ == "stack" ? QString("LIFO 栈指针 top = %1").arg(step_)
                                         : QString("活跃指针聚焦位置 [%1]").arg(step_ + 1))));
        }
        return QStringLiteral("时间复杂度对比 · 1s ≈ 10^8 次运算");
    }

    ThemePalette pal_ = get_theme_palette("white");
    QString lesson_id_ = "2.1";
    QString viz_type_ = "complexity";
    QString caption_;
    int step_ = 0;
};

// ============================================================================
// 6. Custom-Painted Chapter/Lesson Catalog Delegate (Rounded Cards + Pills)
// ============================================================================
class DsCatalogDelegate : public QStyledItemDelegate {
public:
    explicit DsCatalogDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setPaletteTheme(const ThemePalette& pal) { pal_ = pal; }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex& index) const override {
        bool isLesson = index.parent().isValid();
        return QSize(0, isLesson ? 34 : 40);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool isLesson = index.parent().isValid();
        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered  = (option.state & QStyle::State_MouseOver);
        const bool expanded = (option.state & QStyle::State_Open);

        const QString title = index.data(Qt::DisplayRole).toString();
        const QString badge = index.data(Qt::UserRole + 1).toString();
        const bool done     = index.data(Qt::UserRole + 2).toBool();

        if (!isLesson) {
            QRectF r = QRectF(option.rect).adjusted(2, 3, -2, -3);
            QColor bg = expanded
                ? (pal_.is_dark ? QColor("#1D2A3F") : QColor("#EFF6FF"))
                : (hovered ? QColor(pal_.bg_subtle) : QColor(pal_.bg_panel));
            QColor bd = expanded
                ? (pal_.is_dark ? QColor("#3B82F6") : QColor("#BFDBFE"))
                : QColor(pal_.border_line);

            painter->setBrush(bg);
            painter->setPen(QPen(bd, 1.0));
            painter->drawRoundedRect(r, 9, 9);

            // Expand indicator triangle/chevron
            QFont f = option.font;
            f.setBold(true);
            f.setPointSizeF(9.5);
            painter->setFont(f);
            painter->setPen(QColor(expanded ? pal_.blue : pal_.text_muted));
            painter->drawText(QRectF(r.left() + 8, r.top(), 16, r.height()),
                              Qt::AlignCenter, expanded ? QStringLiteral("▾") : QStringLiteral("▸"));

            // Right pill badge
            int pillW = 0;
            if (!badge.isEmpty()) {
                QFont pf = option.font; pf.setPointSizeF(8.0); pf.setBold(true);
                QFontMetrics pfm(pf);
                pillW = pfm.horizontalAdvance(badge) + 16;
                QRectF pr(r.right() - pillW - 8, r.center().y() - 10, pillW, 20);
                painter->setBrush(pal_.is_dark ? QColor("#243652") : QColor("#DBEAFE"));
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(pr, 9, 9);
                painter->setFont(pf);
                painter->setPen(QColor(pal_.blue));
                painter->drawText(pr, Qt::AlignCenter, badge);
            }

            // Chapter title
            painter->setFont(f);
            painter->setPen(QColor(expanded ? pal_.blue : pal_.text_primary));
            QRectF tr(r.left() + 25, r.top(), std::max(40.0, r.width() - 36 - pillW), r.height());
            QFontMetrics fm(f);
            painter->drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
                              fm.elidedText(title, Qt::ElideRight, static_cast<int>(tr.width())));
        } else {
            QRectF r = QRectF(option.rect).adjusted(10, 2, -2, -2);
            QColor bg = selected
                ? (pal_.is_dark ? QColor("#1E3A5F") : QColor("#EFF6FF"))
                : (hovered ? QColor(pal_.bg_subtle) : Qt::transparent);
            if (bg.alpha() > 0) {
                painter->setBrush(bg);
                painter->setPen(selected ? QPen(QColor(pal_.is_dark ? "#3B82F6" : "#BFDBFE"), 1.0) : Qt::NoPen);
                painter->drawRoundedRect(r, 8, 8);
            }
            if (selected) {
                QPainterPath clip;
                clip.addRoundedRect(r, 8, 8);
                painter->save();
                painter->setClipPath(clip);
                painter->fillRect(QRectF(r.left(), r.top(), 4, r.height()), QColor(pal_.blue));
                painter->restore();
            }

            int pillW = 0;
            if (!badge.isEmpty()) {
                QFont pf = option.font; pf.setPointSizeF(7.8); pf.setBold(true);
                QFontMetrics pfm(pf);
                pillW = pfm.horizontalAdvance(badge) + 14;
                QRectF pr(r.right() - pillW - 7, r.center().y() - 9.5, pillW, 19);
                QColor pbg = done
                    ? (pal_.is_dark ? QColor("#064E3B") : QColor("#DCFCE7"))
                    : (pal_.is_dark ? QColor("#451A03") : QColor("#FEF3C7"));
                QColor pfg = done ? QColor(pal_.emerald) : QColor(pal_.amber);
                painter->setBrush(pbg);
                painter->setPen(Qt::NoPen);
                painter->drawRoundedRect(pr, 9, 9);
                painter->setFont(pf);
                painter->setPen(pfg);
                painter->drawText(pr, Qt::AlignCenter, badge);
            }

            QFont lf = option.font;
            lf.setPointSizeF(9.0);
            lf.setBold(selected);
            painter->setFont(lf);
            painter->setPen(QColor(selected ? (pal_.is_dark ? "#93C5FD" : "#1D4ED8") : pal_.text_secondary));
            QRectF tr(r.left() + 12, r.top(), std::max(40.0, r.width() - 24 - pillW), r.height());
            QFontMetrics fm(lf);
            painter->drawText(tr, Qt::AlignLeft | Qt::AlignVCenter,
                              fm.elidedText(title, Qt::ElideRight, static_cast<int>(tr.width())));
        }

        painter->restore();
    }

private:
    ThemePalette pal_ = get_theme_palette("white");
};

// ============================================================================
// 7. Custom-Painted Dictionary Entry Delegate (Word + IPA + Tags + Meaning)
// ============================================================================
class DictItemDelegate : public QStyledItemDelegate {
public:
    explicit DictItemDelegate(QObject* parent = nullptr) : QStyledItemDelegate(parent) {}

    void setPaletteTheme(const ThemePalette& pal) { pal_ = pal; }

    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override {
        return QSize(0, 44);
    }

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override {
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);

        const bool selected = (option.state & QStyle::State_Selected);
        const bool hovered  = (option.state & QStyle::State_MouseOver);

        QRectF r = QRectF(option.rect).adjusted(3, 2, -3, -2);
        QColor bg = selected
            ? (pal_.is_dark ? QColor("#1E3A5F") : QColor("#EFF6FF"))
            : (hovered ? QColor(pal_.bg_subtle) : QColor(pal_.bg_card));
        QColor bd = selected ? QColor(pal_.blue) : QColor(pal_.border_line);
        painter->setBrush(bg);
        painter->setPen(QPen(bd, selected ? 1.3 : 0.8));
        painter->drawRoundedRect(r, 8, 8);

        const QString word = index.data(Qt::DisplayRole).toString();
        const QString ipa  = index.data(Qt::UserRole).toString();
        const QString tags = index.data(Qt::UserRole + 1).toString();
        const QString pos  = index.data(Qt::UserRole + 2).toString();
        const bool marked  = index.data(Qt::UserRole + 3).toBool();

        int tagW = 0;
        if (!tags.isEmpty()) {
            QString shortTag = tags.section(' ', 0, 0);
            QFont tf = option.font; tf.setPointSizeF(7.5); tf.setBold(true);
            QFontMetrics tfm(tf);
            tagW = tfm.horizontalAdvance(shortTag) + 12;
            QRectF tr(r.right() - tagW - 8, r.top() + 7, tagW, 17);
            painter->setBrush(pal_.is_dark ? QColor("#2D2250") : QColor("#F3E8FF"));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(tr, 7, 7);
            painter->setFont(tf);
            painter->setPen(QColor(pal_.purple));
            painter->drawText(tr, Qt::AlignCenter, shortTag);
        }

        QFont wf = option.font; wf.setBold(true); wf.setPointSizeF(9.5);
        painter->setFont(wf);
        painter->setPen(QColor(selected ? pal_.blue : pal_.text_primary));
        QString head = marked ? (QStringLiteral("★ ") + word) : word;
        QFontMetrics wfm(wf);
        int hw = wfm.horizontalAdvance(head);
        painter->drawText(QRectF(r.left() + 10, r.top() + 4, std::max(40.0, r.width() - 22 - tagW), 19),
                          Qt::AlignLeft | Qt::AlignVCenter, head);

        if (!ipa.isEmpty()) {
            QFont ipf = option.font; ipf.setPointSizeF(8.2);
            painter->setFont(ipf);
            painter->setPen(QColor(pal_.text_muted));
            double ix = r.left() + 14 + hw;
            double iw = r.right() - tagW - 14 - ix;
            if (iw > 28) {
                painter->drawText(QRectF(ix, r.top() + 4, iw, 19), Qt::AlignLeft | Qt::AlignVCenter,
                                  QString("[%1]").arg(ipa));
            }
        }

        if (!pos.isEmpty()) {
            QFont pf = option.font; pf.setPointSizeF(8.0);
            QFontMetrics pfm(pf);
            painter->setFont(pf);
            painter->setPen(QColor(pal_.text_secondary));
            QString flat = pos; flat.replace('\n', ' ');
            QRectF pr(r.left() + 10, r.top() + 23, r.width() - 20, 16);
            painter->drawText(pr, Qt::AlignLeft | Qt::AlignVCenter,
                              pfm.elidedText(flat, Qt::ElideRight, static_cast<int>(pr.width())));
        }

        painter->restore();
    }

private:
    ThemePalette pal_ = get_theme_palette("white");
};

// ============================================================================
// 8. A4 Document Ruler & Print-Crop-Mark Paper Sheet Widgets
// ============================================================================
class A4RulerWidget : public QWidget {
public:
    explicit A4RulerWidget(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(22);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const double w = width();
        p.setPen(QPen(QColor("#CBD5E1"), 1));
        p.drawLine(0, height() - 1, width(), height() - 1);

        QFont f(QStringLiteral("Consolas"));
        f.setPointSizeF(7.5);
        p.setFont(f);

        // Map 0..16 cm across ruler width
        for (int mm = 0; mm <= 160; mm += 2) {
            double x = 4.0 + (w - 8.0) * (mm / 160.0);
            bool major = (mm % 20 == 0);
            bool mid = (mm % 10 == 0);
            int tickH = major ? 7 : (mid ? 5 : 3);
            p.setPen(QPen(QColor(major ? "#64748B" : "#CBD5E1"), 1));
            p.drawLine(QPointF(x, height() - 1 - tickH), QPointF(x, height() - 1));
            if (major && mm <= 140) {
                p.setPen(QColor("#64748B"));
                p.drawText(QRectF(x - 10, 1, 24, 12), Qt::AlignCenter, QString::number(mm / 10));
            }
        }
        p.setPen(QColor("#94A3B8"));
        p.drawText(QRectF(w - 52, 0, 44, 12), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("15.9cm"));

        // Small blue indent markers at left/right bottom edge
        p.setBrush(QColor("#2563EB"));
        p.setPen(Qt::NoPen);
        QPolygonF leftM;
        leftM << QPointF(1, height() - 7) << QPointF(7, height() - 7) << QPointF(4, height() - 2);
        p.drawPolygon(leftM);
        QPolygonF rightM;
        rightM << QPointF(w - 7, height() - 7) << QPointF(w - 1, height() - 7) << QPointF(w - 4, height() - 2);
        p.drawPolygon(rightM);
    }
};

class A4PaperWidget : public QFrame {
public:
    explicit A4PaperWidget(QWidget* parent = nullptr) : QFrame(parent) {}

    void setMarginsPx(int mx, int my) {
        mx_ = mx;
        my_ = my;
        update();
    }

protected:
    void paintEvent(QPaintEvent* e) override {
        QFrame::paintEvent(e);
        QPainter p(this);
        p.setPen(QPen(QColor("#CBD5E1"), 1.1));
        const int x0 = std::max(16, mx_ - 12);
        const int x1 = std::min(width() - 16, width() - mx_ + 12);
        const int y0 = std::max(14, my_ - 8);
        const int y1 = std::min(height() - 14, height() - my_ + 8);
        const int arm = 12;
        // 4 L-shaped Word crop marks
        p.drawLine(x0, y0, x0 - arm, y0); p.drawLine(x0, y0, x0, y0 - arm);
        p.drawLine(x1, y0, x1 + arm, y0); p.drawLine(x1, y0, x1, y0 - arm);
        p.drawLine(x0, y1, x0 - arm, y1); p.drawLine(x0, y1, x0, y1 + arm);
        p.drawLine(x1, y1, x1 + arm, y1); p.drawLine(x1, y1, x1, y1 + arm);
    }

private:
    int mx_ = 64, my_ = 36;
};

} // namespace omni::qt
