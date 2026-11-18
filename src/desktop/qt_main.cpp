#include "../core/config.hpp"
#include "../core/dict_manager.hpp"
#include "../core/docx_converter.hpp"
#include "../core/ds_manager.hpp"
#include "../core/markdown.hpp"
#include "../core/pdf_converter.hpp"
#include "../core/platform_utils.hpp"
#include "../core/vault_manager.hpp"
#include "qt_doc_utils.hpp"
#include "qt_theme.hpp"
#include "qt_widgets.hpp"

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDate>
#include <QDir>
#include <QFileDialog>
#include <QFileSystemWatcher>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMetaObject>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QSplitter>
#include <QStackedWidget>
#include <QStyleFactory>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QTextList>
#include <QTimer>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace omni::qt {

namespace fs = std::filesystem;

static std::string lower_ext_cli(const std::string& path) {
    std::string ext = omni::path_to_utf8(omni::utf8_to_path(path).extension());
    for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return ext;
}

static int run_cli_convert(const std::string& input_path, const std::string& to_fmt, const std::string& out_path) {
    std::string ext = lower_ext_cli(input_path);
    std::string err;
    std::string final_out = out_path;
    if (final_out.empty()) {
        fs::path p = omni::utf8_to_path(input_path);
        final_out = omni::path_to_utf8(p.parent_path() / (omni::path_to_utf8(p.stem()) + "." + to_fmt));
    }
    bool ok = false;
    if (to_fmt == "docx") {
        std::string md;
        if (ext == ".docx") ok = omni::docx_to_markdown(input_path, md, &err) && omni::markdown_to_docx(md, final_out, &err);
        else ok = omni::read_file_utf8(input_path, md, &err) && omni::markdown_to_docx(md, final_out, &err);
    } else if (to_fmt == "md") {
        std::string md;
        if (ext == ".docx") ok = omni::docx_to_markdown(input_path, md, &err) && omni::write_file_utf8(final_out, md, &err);
        else ok = omni::read_file_utf8(input_path, md, &err) && omni::write_file_utf8(final_out, md, &err);
    } else if (to_fmt == "pdf") {
        if (ext == ".docx") ok = omni::docx_to_pdf(input_path, final_out, &err);
        else {
            std::string md;
            std::string title = omni::path_to_utf8(omni::utf8_to_path(input_path).stem());
            ok = omni::read_file_utf8(input_path, md, &err) && omni::markdown_to_pdf(md, final_out, title, &err);
        }
    } else {
        std::cerr << "Unsupported format: " << to_fmt << "\n";
        return 1;
    }
    if (!ok) { std::cerr << "Conversion failed: " << err << "\n"; return 1; }
    std::cout << "Converted successfully -> " << final_out << "\n";
    return 0;
}

class OmniQtWindow : public QMainWindow {
public:
    explicit OmniQtWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        cfg_.load();
        dict_.init();
        ds_.init();
        if (!cfg_.get().vault_path.empty()) {
            vault_.set_root(cfg_.get().vault_path);
        }

        setWindowTitle(QStringLiteral("OmniWorkbench (全能工作台) — Qt 6 Native"));
        resize(1400, 880);
        setMinimumSize(1120, 700);
        loadWindowIcon();

        buildUi();
        applyTheme(cfg_.get().theme.empty() ? "white" : cfg_.get().theme, false);

        // Switch to persisted tab cleanly
        std::string startTab = cfg_.get().active_tab;
        if (startTab == "knowledge") switchTab(1, false);
        else if (startTab == "ds") switchTab(2, false);
        else if (startTab == "tools") switchTab(3, false);
        else switchTab(0, false);

        refreshDictList();
        refreshReadingList();
        refreshVaultTree();
        refreshDsCatalog(true);
        setupVaultWatcher();
    }

    int dsTotalLessons() const { return ds_.total_lessons(); }
    int dsTotalExercises() const { return ds_.total_exercises(); }
    int dictTotalCount() const { return dict_.total_count(); }

    void applyTheme(const std::string& themeId, bool persist = true) {
        std::string norm = (themeId == "sepia" || themeId == "brown") ? "sepia"
                         : ((themeId == "night" || themeId == "dark") ? "night" : "white");
        pal_ = get_theme_palette(norm);
        qApp->setStyleSheet(build_qt_stylesheet(pal_));

        for (int i = 0; i < 3; ++i) {
            if (theme_btns_[i]) {
                bool chk = (i == 0 && norm == "white") || (i == 1 && norm == "sepia") || (i == 2 && norm == "night");
                theme_btns_[i]->setChecked(chk);
            }
        }
        for (QFrame* c : cards_) apply_card_shadow(c, pal_.is_dark, 20, 3, 16);
        if (word_paper_frame_) apply_card_shadow(word_paper_frame_, pal_.is_dark, 32, 6, 36);
        if (cube_widget_) cube_widget_->setPaletteTheme(pal_);
        if (vault_arc_) vault_arc_->setPaletteTheme(pal_);
        if (ds_viz_) ds_viz_->setPaletteTheme(pal_);
        if (ds_cat_delegate_) {
            ds_cat_delegate_->setPaletteTheme(pal_);
            if (ds_tree_) ds_tree_->viewport()->update();
        }
        if (dict_delegate_) {
            dict_delegate_->setPaletteTheme(pal_);
            if (dict_list_) dict_list_->viewport()->update();
        }
        if (ds_tut_widget_) {
            ds_tut_widget_->setPaletteTheme(pal_);
        }
        if (!cur_word_entry_.word.empty()) showWordDetail(cur_word_entry_.word);
        updateWordStageBackdrop();
        if (persist) {
            cfg_.mutable_get().theme = norm;
            cfg_.save();
        }
    }

    void switchTab(int idx, bool persist = true) {
        if (idx < 0 || idx > 3) idx = 0;
        if (main_stack_) main_stack_->setCurrentIndex(idx);
        for (int i = 0; i < 4; ++i) {
            if (nav_btns_[i]) nav_btns_[i]->setChecked(i == idx);
        }
        if (idx == 1) updateWordPaperScale();
        if (persist) {
            static const char* names[4] = {"english", "knowledge", "ds", "tools"};
            cfg_.mutable_get().active_tab = names[idx];
            cfg_.save();
        }
    }

protected:
    void resizeEvent(QResizeEvent* e) override {
        QMainWindow::resizeEvent(e);
        if (main_stack_ && main_stack_->currentIndex() == 1 && word_fit_mode_ != "custom") {
            updateWordPaperScale();
        }
    }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (watched == word_scroll_->viewport() && event->type() == QEvent::Wheel) {
            auto* we = static_cast<QWheelEvent*>(event);
            if (we->modifiers() & Qt::ControlModifier) {
                stepWordZoom(we->angleDelta().y() > 0 ? 0.10 : -0.10);
                return true;
            }
        }
        return QMainWindow::eventFilter(watched, event);
    }

private:
    ConfigManager cfg_;
    DictManager dict_;
    VaultManager vault_;
    DsManager ds_;
    std::mutex ds_mutex_;
    ThemePalette pal_;

    // Shell widgets
    QToolButton* nav_btns_[4]{};
    QPushButton* theme_btns_[3]{};
    QStackedWidget* main_stack_ = nullptr;
    QLabel* status_toast_ = nullptr;
    QTimer* toast_timer_ = nullptr;

    // Page 0: English
    Cube3DWidget* cube_widget_ = nullptr;
    QLineEdit* dict_search_ = nullptr;
    QComboBox* dict_tag_combo_ = nullptr;
    QPushButton* dict_marked_btn_ = nullptr;
    QListWidget* dict_list_ = nullptr;
    DictItemDelegate* dict_delegate_ = nullptr;
    QLabel* dict_count_lbl_ = nullptr;
    QTabWidget* eng_right_tabs_ = nullptr;
    QLabel* word_title_lbl_ = nullptr;
    QLabel* word_ipa_lbl_ = nullptr;
    QLabel* word_tags_lbl_ = nullptr;
    QPushButton* word_mark_btn_ = nullptr;
    QPushButton* word_fav_btn_ = nullptr;
    QTextBrowser* word_detail_browser_ = nullptr;
    QPlainTextEdit* word_note_edit_ = nullptr;
    std::vector<DictEntry> cur_dict_results_;
    DictEntry cur_word_entry_;
    int dict_offset_ = 0;

    QLineEdit* rd_search_ = nullptr;
    QListWidget* rd_list_ = nullptr;
    QTextBrowser* rd_passage_ = nullptr;
    std::vector<ReadingArticle> cur_readings_;
    ReadingArticle cur_reading_;
    bool rd_show_zh_ = false;

    // Page 1: Knowledge & Word A4 Editor
    VaultArcWidget* vault_arc_ = nullptr;
    QLabel* vault_path_lbl_ = nullptr;
    QFileSystemWatcher* vault_watcher_ = nullptr;
    QLabel* doc_name_lbl_ = nullptr;
    QLabel* doc_dirty_lbl_ = nullptr;
    QPushButton* ed_mode_btns_[3]{};
    QStackedWidget* ed_mode_stack_ = nullptr;
    QScrollArea* word_scroll_ = nullptr;
    QWidget* word_scroll_content_ = nullptr;
    A4PaperWidget* word_paper_frame_ = nullptr;
    A4RulerWidget* word_ruler_widget_ = nullptr;
    QTextEdit* word_editor_ = nullptr;
    QLabel* word_footer_lbl_ = nullptr;
    QPlainTextEdit* md_editor_ = nullptr;
    QPlainTextEdit* tex_box_ = nullptr;
    QListWidget* tex_list_ = nullptr;
    QComboBox* wps_style_combo_ = nullptr;
    QComboBox* wps_font_combo_ = nullptr;
    QComboBox* wps_size_combo_ = nullptr;
    QComboBox* wps_spacing_combo_ = nullptr;
    QPushButton* word_zoom_pct_btn_ = nullptr;
    QPushButton* word_status_zoom_btn_ = nullptr;
    QSlider* word_zoom_slider_ = nullptr;
    QPushButton* word_fit_width_btn_ = nullptr;
    QPushButton* word_fit_page_btn_ = nullptr;
    QSpinBox* word_page_spin_ = nullptr;
    QLabel* word_total_pages_lbl_ = nullptr;
    QLabel* word_char_lbl_ = nullptr;
    std::string active_doc_path_;
    std::string cur_doc_markdown_;
    std::string cur_ed_mode_ = "word"; // "word" | "md" | "latex"
    std::string word_fit_mode_ = "width"; // "width" | "page" | "custom"
    double word_custom_zoom_ = 1.05;
    double word_eff_zoom_ = 1.05;
    int word_total_pages_ = 1;
    bool doc_dirty_ = false;
    bool doc_updating_ = false;

    // Page 2: C/C++ Data Structures & Algorithms
    QLabel* ds_prog_pill_ = nullptr;
    QLabel* ds_db_pill_ = nullptr;
    QLineEdit* ds_search_inp_ = nullptr;
    std::vector<QPushButton*> ds_cat_btns_;
    std::string ds_cat_filter_;
    QTreeWidget* ds_tree_ = nullptr;
    DsCatalogDelegate* ds_cat_delegate_ = nullptr;
    DsVisualizerWidget* ds_viz_ = nullptr;
    QLabel* ds_viz_title_lbl_ = nullptr;
    QLabel* ds_viz_cap_ = nullptr;
    QLabel* ds_viz_step_lbl_ = nullptr;
    QPushButton* ds_viz_play_btn_ = nullptr;
    QTimer* ds_viz_timer_ = nullptr;
    QLabel* ds_les_title_ = nullptr;
    QLabel* ds_les_time_ = nullptr;
    QLabel* ds_les_space_ = nullptr;
    QLabel* ds_les_src_ = nullptr;
    QPushButton* ds_mastered_btn_ = nullptr;
    QFrame* ds_note_drawer_ = nullptr;
    QLineEdit* ds_note_inp_ = nullptr;
    DsTutorialWidget* ds_tut_widget_ = nullptr;
    CodeEditor* ds_tmpl_editor_ = nullptr;
    QHBoxLayout* ds_ex_tabs_layout_ = nullptr;
    QLabel* ds_ex_mode_pill_ = nullptr;
    QLabel* ds_ex_status_pill_ = nullptr;
    QLabel* ds_ex_prompt_lbl_ = nullptr;
    QLabel* ds_ex_sample_lbl_ = nullptr;
    CodeEditor* ds_ide_editor_ = nullptr;
    QLineEdit* ds_stdin_inp_ = nullptr;
    QPushButton* ds_run_btn_ = nullptr;
    QPushButton* ds_submit_btn_ = nullptr;
    QTextBrowser* ds_console_ = nullptr;
    DsLesson cur_ds_lesson_;
    std::string cur_ex_id_;
    QString ds_free_code_ = QStringLiteral("#include <iostream>\n#include <vector>\n#include <algorithm>\nusing namespace std;\n\nint main() {\n    cout << \"Hello C++ Data Structures!\\n\";\n    return 0;\n}");

    // Page 3: Tools
    QLineEdit* tool_in_path_ = nullptr;
    QComboBox* tool_fmt_combo_ = nullptr;
    QLabel* tool_diag_lbl_ = nullptr;
    std::vector<QFrame*> cards_;

    void showToast(const QString& msg) {
        if (!status_toast_) return;
        status_toast_->setText(msg);
        status_toast_->setVisible(true);
        if (!toast_timer_) {
            toast_timer_ = new QTimer(this);
            toast_timer_->setSingleShot(true);
            connect(toast_timer_, &QTimer::timeout, this, [this]() {
                if (status_toast_) status_toast_->setVisible(false);
            });
        }
        toast_timer_->start(2600);
    }

    void loadWindowIcon() {
        QString exeDir = QCoreApplication::applicationDirPath();
        QStringList cands = {
            exeDir + "/app_icon.png",
            exeDir + "/assets/app_icon.png",
            exeDir + "/../assets/app_icon.png",
            exeDir + "/../../assets/app_icon.png"
        };
        for (const QString& p : cands) {
            if (QFile::exists(p)) {
                setWindowIcon(QIcon(p));
                return;
            }
        }
    }

    QFrame* makeCard(QWidget* parent = nullptr) {
        auto* f = new QFrame(parent);
        f->setProperty("card", true);
        apply_card_shadow(f, pal_.is_dark, 20, 3, 16);
        cards_.push_back(f);
        return f;
    }

    void buildUi() {
        auto* central = new QWidget(this);
        central->setObjectName(QStringLiteral("RootCentral"));
        setCentralWidget(central);

        auto* rootV = new QVBoxLayout(central);
        rootV->setContentsMargins(0, 0, 0, 0);
        rootV->setSpacing(0);

        // Top Header Bar
        auto* topBar = new QWidget(central);
        topBar->setObjectName(QStringLiteral("TopHeaderBar"));
        topBar->setFixedHeight(48);
        auto* topL = new QHBoxLayout(topBar);
        topL->setContentsMargins(14, 0, 14, 0);
        topL->setSpacing(10);

        auto* logoBadge = new QLabel(QStringLiteral("OW"), topBar);
        logoBadge->setFixedSize(28, 28);
        logoBadge->setAlignment(Qt::AlignCenter);
        logoBadge->setStyleSheet(QStringLiteral("background:#111827;color:#FFFFFF;border-radius:8px;font-weight:800;font-size:11px;"));

        auto* titleLbl = new QLabel(QStringLiteral("OmniWorkbench (全能工作台)"), topBar);
        titleLbl->setStyleSheet(QStringLiteral("font-weight:700;font-size:14px;"));

        auto* techPill = new QLabel(QStringLiteral("Qt 6 Native · C++17 · SQLite3 · MD/DOCX/PDF"), topBar);
        techPill->setProperty("pill", QStringLiteral("blue"));

        status_toast_ = new QLabel(topBar);
        status_toast_->setProperty("pill", QStringLiteral("green"));
        status_toast_->setVisible(false);

        topL->addWidget(logoBadge);
        topL->addWidget(titleLbl);
        topL->addWidget(techPill);
        topL->addSpacing(8);
        topL->addWidget(status_toast_);
        topL->addStretch();

        const char* tNames[3] = {"● 纯白墨黑", "● 暖棕书卷", "● 极夜黑"};
        const char* tIds[3] = {"white", "sepia", "night"};
        for (int i = 0; i < 3; ++i) {
            auto* tb = new QPushButton(QString::fromUtf8(tNames[i]), topBar);
            tb->setProperty("btn", QStringLiteral("pill"));
            tb->setCheckable(true);
            connect(tb, &QPushButton::clicked, this, [this, id = std::string(tIds[i])]() { applyTheme(id, true); });
            theme_btns_[i] = tb;
            topL->addWidget(tb);
        }
        rootV->addWidget(topBar);

        // Main Body: Left Rail + Stack
        auto* bodyRow = new QHBoxLayout();
        bodyRow->setContentsMargins(0, 0, 0, 0);
        bodyRow->setSpacing(0);

        auto* rail = new QWidget(central);
        rail->setObjectName(QStringLiteral("LeftNavRail"));
        rail->setFixedWidth(70);
        auto* railL = new QVBoxLayout(rail);
        railL->setContentsMargins(6, 12, 6, 12);
        railL->setSpacing(8);

        const char* navLabels[4] = {"A\nEnglish", "📘\n知识", "{ }\n数据结构", "⚙\n工具"};
        for (int i = 0; i < 4; ++i) {
            auto* nb = new QToolButton(rail);
            nb->setProperty("nav", true);
            nb->setText(QString::fromUtf8(navLabels[i]));
            nb->setCheckable(true);
            nb->setFixedWidth(58);
            nb->setMinimumHeight(54);
            connect(nb, &QToolButton::clicked, this, [this, i]() { switchTab(i, true); });
            nav_btns_[i] = nb;
            railL->addWidget(nb);
        }
        railL->addStretch();
        bodyRow->addWidget(rail);

        main_stack_ = new QStackedWidget(central);
        main_stack_->addWidget(buildEnglishPage());   // 0
        main_stack_->addWidget(buildKnowledgePage()); // 1
        main_stack_->addWidget(buildDsPage());        // 2
        main_stack_->addWidget(buildToolsPage());     // 3
        bodyRow->addWidget(main_stack_, 1);

        rootV->addLayout(bodyRow, 1);
    }

    // ========================================================================
    // PAGE 0: English 3D Cube + SQLite3 Dictionary + Reading Trainer
    // ========================================================================
    QWidget* buildEnglishPage() {
        auto* page = new QWidget();
        auto* sp = new QSplitter(Qt::Horizontal, page);
        auto* lay = new QHBoxLayout(page);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->addWidget(sp);

        // Left: 3D Cube + Dictionary list
        auto* leftW = new QWidget();
        auto* leftL = new QVBoxLayout(leftW);
        leftL->setContentsMargins(0, 0, 0, 0);
        leftL->setSpacing(8);

        auto* cubeCard = makeCard(leftW);
        auto* cubeL = new QVBoxLayout(cubeCard);
        cubeL->setContentsMargins(8, 8, 8, 8);
        cubeL->setSpacing(6);

        auto* cubeHead = new QHBoxLayout();
        auto* cTitle = new QLabel(QStringLiteral("🧊 交互式 3D 词根字母魔方"), cubeCard);
        cTitle->setStyleSheet(QStringLiteral("font-weight:700;font-size:12.5px;"));
        auto* btn3 = new QPushButton(QStringLiteral("3×3"), cubeCard); btn3->setProperty("btn", "pill");
        auto* btn2 = new QPushButton(QStringLiteral("2×2音标"), cubeCard); btn2->setProperty("btn", "pill");
        auto* btnSpin = new QPushButton(QStringLiteral("⏯ 自转"), cubeCard); btnSpin->setProperty("btn", "pill");
        auto* btnReset = new QPushButton(QStringLiteral("↺ 复位"), cubeCard); btnReset->setProperty("btn", "pill");
        cubeHead->addWidget(cTitle);
        cubeHead->addStretch();
        cubeHead->addWidget(btn3);
        cubeHead->addWidget(btn2);
        cubeHead->addWidget(btnSpin);
        cubeHead->addWidget(btnReset);
        cubeL->addLayout(cubeHead);

        cube_widget_ = new Cube3DWidget(cubeCard);
        cubeL->addWidget(cube_widget_, 1);

        connect(btn3, &QPushButton::clicked, this, [this]() { cube_widget_->setMode(3); });
        connect(btn2, &QPushButton::clicked, this, [this]() { cube_widget_->setMode(2); });
        connect(btnSpin, &QPushButton::clicked, this, [this, btnSpin]() {
            cube_widget_->toggleAutoRotate();
            btnSpin->setText(cube_widget_->isAutoRotating() ? QStringLiteral("⏸ 暂停") : QStringLiteral("⏯ 自转"));
        });
        connect(btnReset, &QPushButton::clicked, this, [this]() { cube_widget_->resetCamera(); });
        cube_widget_->onCellSelected = [this](const QString& lbl, bool isPhonetic) {
            if (!isPhonetic && dict_search_) {
                dict_search_->setText(lbl.toLower());
                dict_offset_ = 0;
                refreshDictList();
            } else if (isPhonetic) {
                QString clean = lbl; clean.remove('/');
                auto vec = dict_.search_by_phonetic(lbl.toStdString(), clean.toStdString(), 80, 0);
                populateDictResults(vec, static_cast<int>(vec.size()));
            }
        };
        leftL->addWidget(cubeCard, 1);

        auto* dictCard = makeCard(leftW);
        auto* dictL = new QVBoxLayout(dictCard);
        dictL->setContentsMargins(9, 8, 9, 8);
        dictL->setSpacing(6);

        auto* dHead = new QHBoxLayout();
        auto* dLbl = new QLabel(QStringLiteral("📚 SQLite3 离线词库"), dictCard);
        dLbl->setStyleSheet(QStringLiteral("font-weight:700;"));
        dict_count_lbl_ = new QLabel(QString("共 %1 词").arg(dict_.total_count()), dictCard);
        dict_count_lbl_->setProperty("pill", "blue");
        auto* impBtn = new QPushButton(QStringLiteral("📥 导入词库"), dictCard);
        impBtn->setProperty("btn", "pill");
        connect(impBtn, &QPushButton::clicked, this, [this]() {
            QString fp = QFileDialog::getOpenFileName(this, QStringLiteral("选择开源词库 (JSON/NDJSON/CSV/ZIP)"),
                                                      QString(), QStringLiteral("词库文件 (*.json *.ndjson *.csv *.zip);;所有文件 (*)"));
            if (fp.isEmpty()) return;
            bool okTag = false;
            QString tag = QInputDialog::getText(this, QStringLiteral("词书标签"), QStringLiteral("输入词书标签（如 CET4 / CET6 / 考研）："), QLineEdit::Normal, QStringLiteral("CET6"), &okTag);
            int cnt = 0; std::string err;
            if (dict_.import_dictionary_file(fp.toStdString(), okTag ? tag.toStdString() : "", cnt, &err)) {
                showToast(QString("成功导入 %1 个单词 ✓").arg(cnt));
                refreshDictList();
            } else {
                QMessageBox::warning(this, QStringLiteral("导入失败"), QString::fromStdString(err));
            }
        });
        dHead->addWidget(dLbl);
        dHead->addWidget(dict_count_lbl_);
        dHead->addStretch();
        dHead->addWidget(impBtn);
        dictL->addLayout(dHead);

        auto* filterRow = new QHBoxLayout();
        dict_search_ = new QLineEdit(dictCard);
        dict_search_->setPlaceholderText(QStringLiteral("搜索英文拼写 / 词形 / 中文释义..."));
        dict_tag_combo_ = new QComboBox(dictCard);
        dict_tag_combo_->addItems({"全部", "CET4", "CET6", "考研", "IELTS", "TOEFL"});
        dict_marked_btn_ = new QPushButton(QStringLiteral("★ 已标"), dictCard);
        dict_marked_btn_->setProperty("btn", "pill");
        dict_marked_btn_->setCheckable(true);
        filterRow->addWidget(dict_search_, 1);
        filterRow->addWidget(dict_tag_combo_);
        filterRow->addWidget(dict_marked_btn_);
        dictL->addLayout(filterRow);

        dict_list_ = new QListWidget(dictCard);
        dict_list_->setMouseTracking(true);
        dict_delegate_ = new DictItemDelegate(dict_list_);
        dict_list_->setItemDelegate(dict_delegate_);
        dictL->addWidget(dict_list_, 1);

        auto* pagerRow = new QHBoxLayout();
        auto* prevW = new QPushButton(QStringLiteral("◀ 上页"), dictCard);
        auto* nextW = new QPushButton(QStringLiteral("下页 ▶"), dictCard);
        prevW->setProperty("btn", "pill"); nextW->setProperty("btn", "pill");
        pagerRow->addStretch();
        pagerRow->addWidget(prevW);
        pagerRow->addWidget(nextW);
        dictL->addLayout(pagerRow);
        leftL->addWidget(dictCard, 1);

        connect(dict_search_, &QLineEdit::textChanged, this, [this]() { dict_offset_ = 0; refreshDictList(); });
        connect(dict_tag_combo_, &QComboBox::currentIndexChanged, this, [this]() { dict_offset_ = 0; refreshDictList(); });
        connect(dict_marked_btn_, &QPushButton::clicked, this, [this]() { dict_offset_ = 0; refreshDictList(); });
        connect(prevW, &QPushButton::clicked, this, [this]() {
            dict_offset_ = std::max(0, dict_offset_ - 80); refreshDictList();
        });
        connect(nextW, &QPushButton::clicked, this, [this]() {
            dict_offset_ += 80; refreshDictList();
        });
        connect(dict_list_, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row >= 0 && row < static_cast<int>(cur_dict_results_.size())) {
                showWordDetail(cur_dict_results_[static_cast<size_t>(row)].word);
            }
        });

        // Right: Word Detail & Notes vs Reading Comprehension
        eng_right_tabs_ = new QTabWidget();

        // Tab A: Word Detail & Notes
        auto* wTab = new QWidget();
        auto* wL = new QVBoxLayout(wTab);
        wL->setContentsMargins(0, 4, 0, 0);
        wL->setSpacing(8);

        auto* wCard = makeCard(wTab);
        auto* wcL = new QVBoxLayout(wCard);
        wcL->setContentsMargins(14, 12, 14, 12);
        wcL->setSpacing(8);

        auto* wh = new QHBoxLayout();
        word_title_lbl_ = new QLabel(QStringLiteral("选择一个单词"), wCard);
        word_title_lbl_->setStyleSheet(QStringLiteral("font-size:22px;font-weight:800;"));
        word_ipa_lbl_ = new QLabel(QStringLiteral(""), wCard);
        word_ipa_lbl_->setProperty("secondary", true);
        word_tags_lbl_ = new QLabel(QStringLiteral(""), wCard);
        word_tags_lbl_->setProperty("pill", "purple");
        word_mark_btn_ = new QPushButton(QStringLiteral("☆ 标重点"), wCard);
        word_fav_btn_ = new QPushButton(QStringLiteral("♡ 收藏"), wCard);
        auto* pushVocBtn = new QPushButton(QStringLiteral("✎ 存入知识库"), wCard);
        word_mark_btn_->setProperty("btn", "pill");
        word_fav_btn_->setProperty("btn", "pill");
        pushVocBtn->setProperty("btn", "primary");

        wh->addWidget(word_title_lbl_);
        wh->addWidget(word_ipa_lbl_);
        wh->addWidget(word_tags_lbl_);
        wh->addStretch();
        wh->addWidget(word_mark_btn_);
        wh->addWidget(word_fav_btn_);
        wh->addWidget(pushVocBtn);
        wcL->addLayout(wh);

        word_detail_browser_ = new QTextBrowser(wCard);
        word_detail_browser_->setOpenExternalLinks(false);
        wcL->addWidget(word_detail_browser_, 1);

        auto* nLbl = new QLabel(QStringLiteral("📒 个人学习反思与记忆笔记（SQLite3 持久化）"), wCard);
        nLbl->setStyleSheet(QStringLiteral("font-weight:700;font-size:12px;"));
        word_note_edit_ = new QPlainTextEdit(wCard);
        word_note_edit_->setMaximumHeight(96);
        word_note_edit_->setPlaceholderText(QStringLiteral("在此记下词根助记、真题搭配或易错辨析..."));
        auto* saveNoteBtn = new QPushButton(QStringLiteral("💾 保存笔记"), wCard);
        saveNoteBtn->setProperty("btn", "blue");
        auto* nBtnRow = new QHBoxLayout();
        nBtnRow->addStretch();
        nBtnRow->addWidget(saveNoteBtn);
        wcL->addWidget(nLbl);
        wcL->addWidget(word_note_edit_);
        wcL->addLayout(nBtnRow);
        wL->addWidget(wCard, 1);

        connect(word_mark_btn_, &QPushButton::clicked, this, [this]() {
            if (cur_word_entry_.word.empty()) return;
            cur_word_entry_.marked = !cur_word_entry_.marked;
            dict_.set_marked(cur_word_entry_.word, cur_word_entry_.marked);
            showWordDetail(cur_word_entry_.word);
        });
        connect(word_fav_btn_, &QPushButton::clicked, this, [this]() {
            if (cur_word_entry_.word.empty()) return;
            cur_word_entry_.favorite = !cur_word_entry_.favorite;
            dict_.set_favorite(cur_word_entry_.word, cur_word_entry_.favorite);
            showWordDetail(cur_word_entry_.word);
        });
        connect(saveNoteBtn, &QPushButton::clicked, this, [this]() {
            if (cur_word_entry_.word.empty()) return;
            if (dict_.save_note(cur_word_entry_.word, word_note_edit_->toPlainText().toStdString())) {
                showToast(QStringLiteral("已保存单词笔记到 SQLite3 ✓"));
            }
        });
        connect(pushVocBtn, &QPushButton::clicked, this, [this]() {
            if (cur_word_entry_.word.empty()) return;
            if (!vault_.root_exists()) {
                showToast(QStringLiteral("请先在「知识」界面选择一个本地知识库文件夹"));
                return;
            }
            std::string err;
            if (vault_.append_vocabulary_entry(cur_word_entry_.word, cur_word_entry_.ipa,
                                               cur_word_entry_.pos, word_note_edit_->toPlainText().toStdString(), &err)) {
                refreshVaultTree();
                showToast(QStringLiteral("✎ 已写入知识库 English/Vocabulary.md"));
            } else {
                QMessageBox::warning(this, QStringLiteral("写入失败"), QString::fromStdString(err));
            }
        });

        // Tab B: Reading Comprehension
        auto* rTab = new QWidget();
        auto* rL = new QHBoxLayout(rTab);
        rL->setContentsMargins(0, 4, 0, 0);
        rL->setSpacing(8);

        auto* rListCard = makeCard(rTab);
        rListCard->setFixedWidth(250);
        auto* rlL = new QVBoxLayout(rListCard);
        rlL->setContentsMargins(8, 8, 8, 8);
        rd_search_ = new QLineEdit(rListCard);
        rd_search_->setPlaceholderText(QStringLiteral("搜索真题/阅读文章..."));
        rd_list_ = new QListWidget(rListCard);
        rlL->addWidget(rd_search_);
        rlL->addWidget(rd_list_, 1);

        auto* rBodyCard = makeCard(rTab);
        auto* rbL = new QVBoxLayout(rBodyCard);
        rbL->setContentsMargins(12, 10, 12, 10);
        auto* rbHead = new QHBoxLayout();
        auto* zhToggleBtn = new QPushButton(QStringLiteral("中英对照"), rBodyCard);
        zhToggleBtn->setProperty("btn", "pill");
        zhToggleBtn->setCheckable(true);
        rbHead->addWidget(new QLabel(QStringLiteral("📖 精选英语真题与长文阅读"), rBodyCard));
        rbHead->addStretch();
        rbHead->addWidget(zhToggleBtn);
        rbL->addLayout(rbHead);
        rd_passage_ = new QTextBrowser(rBodyCard);
        rbL->addWidget(rd_passage_, 1);

        rL->addWidget(rListCard);
        rL->addWidget(rBodyCard, 1);

        connect(rd_search_, &QLineEdit::textChanged, this, [this]() { refreshReadingList(); });
        connect(rd_list_, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row >= 0 && row < static_cast<int>(cur_readings_.size())) {
                cur_reading_ = cur_readings_[static_cast<size_t>(row)];
                renderReadingPassage();
            }
        });
        connect(zhToggleBtn, &QPushButton::clicked, this, [this, zhToggleBtn]() {
            rd_show_zh_ = zhToggleBtn->isChecked();
            renderReadingPassage();
        });

        eng_right_tabs_->addTab(wTab, QStringLiteral("📖 单词精讲 & 笔记"));
        eng_right_tabs_->addTab(rTab, QStringLiteral("📰 长文真题阅读"));

        sp->addWidget(leftW);
        sp->addWidget(eng_right_tabs_);
        sp->setStretchFactor(0, 4);
        sp->setStretchFactor(1, 6);
        return page;
    }

    void refreshDictList() {
        std::string q = dict_search_ ? dict_search_->text().trimmed().toStdString() : "";
        std::string tag = dict_tag_combo_ && dict_tag_combo_->currentIndex() > 0
                        ? dict_tag_combo_->currentText().toStdString() : "";
        bool markedOnly = dict_marked_btn_ && dict_marked_btn_->isChecked();
        int tot = dict_.count_matches(q, tag, markedOnly);
        auto res = dict_.search(q, 80, tag, markedOnly, false, dict_offset_);
        populateDictResults(res, tot);
    }

    void populateDictResults(const std::vector<DictEntry>& res, int total) {
        cur_dict_results_ = res;
        if (dict_count_lbl_) dict_count_lbl_->setText(QString("匹配 %1 词").arg(total));
        if (!dict_list_) return;
        dict_list_->clear();
        for (const auto& e : res) {
            auto* item = new QListWidgetItem(QString::fromStdString(e.word));
            item->setData(Qt::UserRole, QString::fromStdString(e.ipa));
            item->setData(Qt::UserRole + 1, QString::fromStdString(e.tags));
            item->setData(Qt::UserRole + 2, QString::fromStdString(e.pos));
            item->setData(Qt::UserRole + 3, e.marked);
            dict_list_->addItem(item);
        }
        if (!res.empty()) {
            dict_list_->setCurrentRow(0);
        }
    }

    void showWordDetail(const std::string& word) {
        DictEntry e;
        if (!dict_.get_word(word, e)) return;
        cur_word_entry_ = e;
        word_title_lbl_->setText(QString::fromStdString(e.word));
        word_ipa_lbl_->setText(e.ipa.empty() ? "" : QString(" /%1/").arg(QString::fromStdString(e.ipa)));
        word_tags_lbl_->setText(QString::fromStdString(e.tags.empty() ? "词汇" : e.tags));
        word_mark_btn_->setText(e.marked ? QStringLiteral("★ 已标重点") : QStringLiteral("☆ 标重点"));
        word_fav_btn_->setText(e.favorite ? QStringLiteral("♥ 已收藏") : QStringLiteral("♡ 收藏"));
        word_note_edit_->setPlainText(QString::fromStdString(e.note));

        QString cardBg = pal_.is_dark ? QStringLiteral("#182232") : QStringLiteral("#F8FAFC");
        QString cardBd = pal_.is_dark ? QStringLiteral("#2B3648") : QStringLiteral("#E2E8F0");
        QString leadBg = pal_.is_dark ? QStringLiteral("#1B2840") : QStringLiteral("#EFF6FF");
        QString leadBd = pal_.is_dark ? QStringLiteral("#2D4A76") : QStringLiteral("#BFDBFE");

        auto wrapCard = [&](const QString& inner, bool isLead = false) {
            return QString("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"10\" "
                           "style=\"background-color:%1; border:1px solid %2; margin-bottom:8px;\"><tr><td>%3</td></tr></table>")
                .arg(isLead ? leadBg : cardBg, isLead ? leadBd : cardBd, inner);
        };

        QString html = QString("<style>body{font-family:'Microsoft YaHei UI';font-size:13px;line-height:1.65;color:%1;} "
                               "b{color:%2;} ul{margin:4px 0 0 18px;}</style>")
                           .arg(pal_.text_primary, pal_.blue);
        if (!e.pos.empty()) {
            html += wrapCard(QString("<b>词性与核心释义</b><br>%1").arg(QString::fromStdString(e.pos).toHtmlEscaped().replace("\n", "<br>")), true);
        }
        if (!e.phrases.empty()) {
            QString inner = QStringLiteral("<b>高频搭配与短语</b><ul>");
            for (const auto& p : e.phrases) {
                inner += QString("<li><span style='font-family:Consolas;font-weight:bold;color:%1;'>%2</span> — %3</li>")
                             .arg(pal_.blue, QString::fromStdString(p.phrase).toHtmlEscaped(), QString::fromStdString(p.translation).toHtmlEscaped());
            }
            inner += QStringLiteral("</ul>");
            html += wrapCard(inner);
        }
        if (!e.sentences.empty()) {
            QString inner = QStringLiteral("<b>双语真题例句</b>");
            for (const auto& s : e.sentences) {
                inner += QString("<p style='margin:6px 0 2px 0;'>%1<br><span style='color:%2;'>%3</span></p>")
                             .arg(QString::fromStdString(s.en).toHtmlEscaped(), pal_.text_secondary, QString::fromStdString(s.zh).toHtmlEscaped());
            }
            html += wrapCard(inner);
        }
        if (!e.rem_method.empty()) {
            html += wrapCard(QString("<b>词根助记</b><br>%1").arg(QString::fromStdString(e.rem_method).toHtmlEscaped()));
        }
        word_detail_browser_->setHtml(html);
    }

    void refreshReadingList() {
        std::string q = rd_search_ ? rd_search_->text().trimmed().toStdString() : "";
        cur_readings_ = dict_.search_readings(q, "", "", 40, 0);
        if (!rd_list_) return;
        rd_list_->clear();
        for (const auto& r : cur_readings_) {
            rd_list_->addItem(QString("[%1] %2").arg(QString::fromStdString(r.source), QString::fromStdString(r.title)));
        }
        if (!cur_readings_.empty()) rd_list_->setCurrentRow(0);
    }

    void renderReadingPassage() {
        if (cur_reading_.id == 0 || !rd_passage_) return;
        QString html = QString("<h2>%1</h2><p style='color:#6B7280;font-size:11px;'>来源：%2 | 类型：%3</p><hr>")
            .arg(QString::fromStdString(cur_reading_.title).toHtmlEscaped(),
                 QString::fromStdString(cur_reading_.source).toHtmlEscaped(),
                 QString::fromStdString(cur_reading_.kind).toHtmlEscaped());
        QString en = QString::fromStdString(cur_reading_.passage_en).toHtmlEscaped().replace("\n", "<br><br>");
        html += QString("<div style='font-family:Georgia,serif;font-size:14px;line-height:1.75;'>%1</div>").arg(en);
        if (rd_show_zh_ && !cur_reading_.passage_zh.empty()) {
            QString zh = QString::fromStdString(cur_reading_.passage_zh).toHtmlEscaped().replace("\n", "<br><br>");
            html += QString("<hr><h4>参考译文</h4><div style='line-height:1.7;'>%1</div>").arg(zh);
        }
        JsonValue qs;
        if (JsonValue::parse(cur_reading_.questions_json, qs) && qs.is_array() && !qs.arr_val.empty()) {
            html += "<hr><h4>随堂阅读理解参考题</h4>";
            for (size_t i = 0; i < qs.arr_val.size(); ++i) {
                const auto& q = qs.arr_val[i];
                html += QString("<p><b>%1. %2</b><br>").arg(i + 1).arg(QString::fromStdString(q.get("question").str_val).toHtmlEscaped());
                if (q.get("options").is_array()) {
                    for (const auto& op : q.get("options").arr_val) {
                        html += QString::fromStdString(op.str_val).toHtmlEscaped() + "<br>";
                    }
                }
                html += QString("<span style='color:#059669;'>参考答案：%1</span></p>").arg(QString::fromStdString(q.get("answer").str_val).toHtmlEscaped());
            }
        }
        rd_passage_->setHtml(html);
    }

    // ========================================================================
    // PAGE 1: Knowledge Vault (Wi-Fi Arcs) + Side-Docked Toolbar + Large A4 Word
    // ========================================================================
    QWidget* buildKnowledgePage() {
        auto* page = new QWidget();
        auto* sp = new QSplitter(Qt::Horizontal, page);
        auto* lay = new QHBoxLayout(page);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->addWidget(sp);

        // Left Pane (31%): Wi-Fi Arc Vault + Markdown Cheat Card
        auto* leftW = new QWidget();
        leftW->setMinimumWidth(300);
        auto* leftL = new QVBoxLayout(leftW);
        leftL->setContentsMargins(0, 0, 0, 0);
        leftL->setSpacing(8);

        auto* vaultCard = makeCard(leftW);
        auto* vcL = new QVBoxLayout(vaultCard);
        vcL->setContentsMargins(8, 8, 8, 8);
        vcL->setSpacing(6);

        auto* hudRow = new QHBoxLayout();
        vault_path_lbl_ = new QLabel(QStringLiteral("📁 未选择"), vaultCard);
        vault_path_lbl_->setProperty("secondary", true);
        auto* pickBtn = new QPushButton(QStringLiteral("选择"), vaultCard); pickBtn->setProperty("btn", "primary");
        auto* newDirBtn = new QPushButton(QStringLiteral("+ 目录"), vaultCard); newDirBtn->setProperty("btn", "pill");
        auto* newNoteBtn = new QPushButton(QStringLiteral("+ 笔记"), vaultCard); newNoteBtn->setProperty("btn", "blue");
        auto* refBtn = new QPushButton(QStringLiteral("↻"), vaultCard); refBtn->setProperty("btn", "pill");
        hudRow->addWidget(vault_path_lbl_, 1);
        hudRow->addWidget(pickBtn);
        hudRow->addWidget(newDirBtn);
        hudRow->addWidget(newNoteBtn);
        hudRow->addWidget(refBtn);
        vcL->addLayout(hudRow);

        vault_arc_ = new VaultArcWidget(vaultCard);
        vcL->addWidget(vault_arc_, 1);
        leftL->addWidget(vaultCard, 6);

        // Markdown Cheat Card
        auto* mdCheatCard = makeCard(leftW);
        auto* mcL = new QVBoxLayout(mdCheatCard);
        mcL->setContentsMargins(9, 8, 9, 8);
        mcL->setSpacing(5);
        auto* mcHead = new QLabel(QStringLiteral("Markdown 简明教程（点击快速插入）"), mdCheatCard);
        mcHead->setStyleSheet(QStringLiteral("font-weight:700;font-size:11.5px;"));
        mcL->addWidget(mcHead);
        auto* mcGrid = new QGridLayout();
        mcGrid->setSpacing(4);
        const struct { const char* lbl; const char* snippet; } tips[10] = {
            {"# 一级标题", "# 一级标题\n"}, {"**粗体** *斜体*", "**粗体** *斜体*"},
            {"- 列表项", "- 列表项\n"}, {"- [ ] 待办", "- [ ] 待办事项\n"},
            {"> 楷体引用", "> 引用内容\n"}, {"`代码块`", "```cpp\n// 代码\n```\n"},
            {"$$公式块$$", "$$E = mc^2$$\n"}, {"| 表格 |", "| 列1 | 列2 |\n| --- | --- |\n| A | B |\n"},
            {"~~删除线~~", "~~删除线~~"}, {"[[双向链接]]", "[[关联笔记]]"}
        };
        for (int i = 0; i < 10; ++i) {
            auto* b = new QPushButton(QString::fromUtf8(tips[i].lbl), mdCheatCard);
            b->setProperty("btn", "pill");
            connect(b, &QPushButton::clicked, this, [this, sn = QString::fromUtf8(tips[i].snippet)]() {
                insertSnippetIntoDoc(sn);
            });
            mcGrid->addWidget(b, i / 2, i % 2);
        }
        mcL->addLayout(mcGrid);
        mcL->addStretch(1);
        leftL->addWidget(mdCheatCard, 4);

        connect(pickBtn, &QPushButton::clicked, this, [this]() { pickVaultFolderQt(); });
        connect(newDirBtn, &QPushButton::clicked, this, [this]() { createNewFolderQt(); });
        connect(newNoteBtn, &QPushButton::clicked, this, [this]() { createNewNoteQt(); });
        connect(refBtn, &QPushButton::clicked, this, [this]() { refreshVaultTree(); showToast(QStringLiteral("已刷新本地知识库目录")); });

        vault_arc_->onItemClicked = [this](const std::string& rel, bool isDir) {
            if (!isDir) openVaultDoc(rel);
        };
        vault_arc_->onItemRightClicked = [this](const std::string& rel, bool, const QPoint& gp) {
            QMenu m(this);
            m.addAction(QStringLiteral("+ 在根目录新建笔记"), [this]() { createNewNoteQt(); });
            m.addAction(QStringLiteral("+ 新建目录"), [this]() { createNewFolderQt(); });
            m.addAction(QStringLiteral("📂 在系统文件夹中打开"), [this]() {
                if (vault_.root_exists()) open_in_file_manager(vault_.root());
            });
            (void)rel;
            m.exec(gp);
        };

        // Right Pane (69%): Side-Docked Toolbar + Large Centered A4 Word Stage
        auto* rightContainer = new QWidget();
        auto* rightH = new QHBoxLayout(rightContainer);
        rightH->setContentsMargins(0, 0, 0, 0);
        rightH->setSpacing(8);

        // Left Vertical Dock (212px)
        auto* dockScroll = new QScrollArea(rightContainer);
        dockScroll->setFixedWidth(212);
        dockScroll->setWidgetResizable(true);
        dockScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto* dockInner = new QWidget();
        auto* dockL = new QVBoxLayout(dockInner);
        dockL->setContentsMargins(2, 2, 4, 2);
        dockL->setSpacing(8);

        // Dock Card 1: Doc actions & Mode
        auto* dCard1 = makeCard(dockInner);
        auto* dc1 = new QVBoxLayout(dCard1);
        dc1->setContentsMargins(8, 8, 8, 8);
        dc1->setSpacing(6);
        auto* dnRow = new QHBoxLayout();
        doc_name_lbl_ = new QLabel(QStringLiteral("未打开文档"), dCard1);
        doc_name_lbl_->setStyleSheet(QStringLiteral("font-weight:700;font-size:11.5px;"));
        doc_dirty_lbl_ = new QLabel(QStringLiteral("●"), dCard1);
        doc_dirty_lbl_->setStyleSheet(QStringLiteral("color:#D97706;font-weight:bold;"));
        doc_dirty_lbl_->setVisible(false);
        dnRow->addWidget(doc_name_lbl_, 1);
        dnRow->addWidget(doc_dirty_lbl_);
        dc1->addLayout(dnRow);

        auto* mRow = new QHBoxLayout();
        mRow->setSpacing(3);
        const char* mNames[3] = {"Word", "MD", "TeX"};
        const char* mIds[3] = {"word", "md", "latex"};
        for (int i = 0; i < 3; ++i) {
            auto* mb = new QPushButton(mNames[i], dCard1);
            mb->setProperty("btn", "seg");
            mb->setCheckable(true);
            connect(mb, &QPushButton::clicked, this, [this, mode = std::string(mIds[i])]() { switchEditorModeQt(mode); });
            ed_mode_btns_[i] = mb;
            mRow->addWidget(mb);
        }
        dc1->addLayout(mRow);

        auto* actRow = new QHBoxLayout();
        auto* saveBtn = new QPushButton(QStringLiteral("💾 保存"), dCard1);
        saveBtn->setProperty("btn", "primary");
        auto* expBtn = new QPushButton(QStringLiteral("导出 ▾"), dCard1);
        expBtn->setProperty("btn", "secondary");
        auto* expMenu = new QMenu(expBtn);
        expMenu->addAction(QStringLiteral("📘 Word (.docx)"), [this]() { exportActiveDocQt("docx", false); });
        expMenu->addAction(QStringLiteral("📝 Markdown (.md)"), [this]() { exportActiveDocQt("md", false); });
        expMenu->addAction(QStringLiteral("📑 A4 PDF (.pdf)"), [this]() { exportActiveDocQt("pdf", false); });
        expMenu->addSeparator();
        expMenu->addAction(QStringLiteral("💾 另存 Word..."), [this]() { exportActiveDocQt("docx", true); });
        expMenu->addAction(QStringLiteral("💾 另存 PDF..."), [this]() { exportActiveDocQt("pdf", true); });
        expBtn->setMenu(expMenu);
        connect(saveBtn, &QPushButton::clicked, this, [this]() { saveActiveDocQt(); });
        actRow->addWidget(saveBtn);
        actRow->addWidget(expBtn);
        dc1->addLayout(actRow);
        dockL->addWidget(dCard1);

        // Dock Card 2: A4 Zoom Controls
        auto* dCard2 = makeCard(dockInner);
        auto* dc2 = new QVBoxLayout(dCard2);
        dc2->setContentsMargins(8, 8, 8, 8);
        dc2->setSpacing(5);
        auto* zHead = new QHBoxLayout();
        auto* zt = new QLabel(QStringLiteral("A4 纸张缩放"), dCard2); zt->setStyleSheet("font-weight:700;font-size:11px;");
        auto* zhint = new QLabel(QStringLiteral("Ctrl+滚轮"), dCard2); zhint->setProperty("muted", true); zhint->setStyleSheet("font-size:10px;");
        zHead->addWidget(zt); zHead->addStretch(); zHead->addWidget(zhint);
        dc2->addLayout(zHead);

        auto* zRow = new QHBoxLayout();
        auto* zOut = new QPushButton(QStringLiteral("−"), dCard2); zOut->setProperty("btn", "pill"); zOut->setFixedWidth(28);
        word_zoom_pct_btn_ = new QPushButton(QStringLiteral("105%"), dCard2); word_zoom_pct_btn_->setProperty("btn", "pill");
        auto* zIn = new QPushButton(QStringLiteral("+"), dCard2); zIn->setProperty("btn", "pill"); zIn->setFixedWidth(28);
        zRow->addWidget(zOut); zRow->addWidget(word_zoom_pct_btn_, 1); zRow->addWidget(zIn);
        dc2->addLayout(zRow);

        word_zoom_slider_ = new QSlider(Qt::Horizontal, dCard2);
        word_zoom_slider_->setRange(50, 180);
        word_zoom_slider_->setValue(105);
        dc2->addWidget(word_zoom_slider_);

        auto* zPreRow = new QHBoxLayout();
        word_fit_width_btn_ = new QPushButton(QStringLiteral("🔍 适宽大图"), dCard2); word_fit_width_btn_->setProperty("btn", "pill"); word_fit_width_btn_->setCheckable(true);
        word_fit_page_btn_ = new QPushButton(QStringLiteral("📄 整页"), dCard2); word_fit_page_btn_->setProperty("btn", "pill"); word_fit_page_btn_->setCheckable(true);
        zPreRow->addWidget(word_fit_width_btn_);
        zPreRow->addWidget(word_fit_page_btn_);
        dc2->addLayout(zPreRow);
        dockL->addWidget(dCard2);

        connect(zOut, &QPushButton::clicked, this, [this]() { stepWordZoom(-0.10); });
        connect(zIn, &QPushButton::clicked, this, [this]() { stepWordZoom(0.10); });
        connect(word_zoom_pct_btn_, &QPushButton::clicked, this, [this]() { setWordCustomZoom(1.0); });
        connect(word_zoom_slider_, &QSlider::valueChanged, this, [this](int val) {
            if (!doc_updating_) setWordCustomZoom(val / 100.0);
        });
        connect(word_fit_width_btn_, &QPushButton::clicked, this, [this]() { word_fit_mode_ = "width"; updateWordPaperScale(); });
        connect(word_fit_page_btn_, &QPushButton::clicked, this, [this]() { word_fit_mode_ = "page"; updateWordPaperScale(); });

        // Dock Card 3: WPS Vertical Formatting Palette
        auto* dCard3 = makeCard(dockInner);
        auto* dc3 = new QVBoxLayout(dCard3);
        dc3->setContentsMargins(8, 8, 8, 8);
        dc3->setSpacing(5);
        auto* fLbl = new QLabel(QStringLiteral("排版与插入工具"), dCard3);
        fLbl->setStyleSheet(QStringLiteral("font-weight:700;font-size:11px;"));
        dc3->addWidget(fLbl);

        wps_style_combo_ = new QComboBox(dCard3);
        wps_style_combo_->addItems({"正文", "H1 黑体", "H2 黑体", "H3 黑体", "H4 黑体", "引用"});
        wps_font_combo_ = new QComboBox(dCard3);
        wps_font_combo_->addItems({"宋体", "黑体", "Times New Roman", "楷体", "微软雅黑", "Consolas"});
        wps_size_combo_ = new QComboBox(dCard3);
        wps_size_combo_->addItems({"42", "36", "26", "22", "18", "16", "14", "12", "10.5", "9"});
        wps_size_combo_->setCurrentText(QStringLiteral("10.5"));
        dc3->addWidget(wps_style_combo_);
        dc3->addWidget(wps_font_combo_);
        dc3->addWidget(wps_size_combo_);

        auto* fmtGrid = new QGridLayout();
        fmtGrid->setSpacing(3);
        auto addFmtBtn = [&](const QString& txt, int r, int c, auto fn) {
            auto* b = new QPushButton(txt, dCard3);
            b->setProperty("btn", "pill");
            connect(b, &QPushButton::clicked, this, fn);
            fmtGrid->addWidget(b, r, c);
        };
        addFmtBtn("B", 0, 0, [this]() { toggleWordCharFormat("bold"); });
        addFmtBtn("I", 0, 1, [this]() { toggleWordCharFormat("italic"); });
        addFmtBtn("U", 0, 2, [this]() { toggleWordCharFormat("underline"); });
        addFmtBtn("S", 0, 3, [this]() { toggleWordCharFormat("strike"); });
        addFmtBtn("`c`", 1, 0, [this]() { toggleWordCharFormat("code"); });
        addFmtBtn("字色", 1, 1, [this]() {
            QColor col = QColorDialog::getColor(QColor("#2563EB"), this, QStringLiteral("选择字体颜色"));
            if (col.isValid()) { QTextCharFormat f; f.setForeground(col); word_editor_->mergeCurrentCharFormat(f); }
        });
        addFmtBtn("高亮", 1, 2, [this]() {
            QColor col = QColorDialog::getColor(QColor("#FEF08A"), this, QStringLiteral("选择高亮底色"));
            if (col.isValid()) { QTextCharFormat f; f.setBackground(col); word_editor_->mergeCurrentCharFormat(f); }
        });
        addFmtBtn("清格式", 1, 3, [this]() {
            QTextCharFormat f; f.setFontWeight(QFont::Normal); f.setFontItalic(false); f.setFontUnderline(false);
            f.setFontStrikeOut(false); f.clearBackground(); f.clearForeground();
            word_editor_->setCurrentCharFormat(f);
        });
        addFmtBtn("左", 2, 0, [this]() { word_editor_->setAlignment(Qt::AlignLeft); });
        addFmtBtn("中", 2, 1, [this]() { word_editor_->setAlignment(Qt::AlignHCenter); });
        addFmtBtn("右", 2, 2, [this]() { word_editor_->setAlignment(Qt::AlignRight); });
        addFmtBtn("•表", 2, 3, [this]() { word_editor_->textCursor().insertList(QTextListFormat::ListDisc); });
        addFmtBtn("1.表", 3, 0, [this]() { word_editor_->textCursor().insertList(QTextListFormat::ListDecimal); });
        addFmtBtn("</>", 3, 1, [this]() { insertSnippetIntoDoc("```cpp\n// 代码\n```\n"); });
        addFmtBtn("∑", 3, 2, [this]() { insertSnippetIntoDoc("$$E = mc^2$$\n"); });
        addFmtBtn("🖼图", 3, 3, [this]() {
            QString fp = QFileDialog::getOpenFileName(this, QStringLiteral("插入图片"), QString(), QStringLiteral("Images (*.png *.jpg *.jpeg *.bmp)"));
            if (!fp.isEmpty()) word_editor_->textCursor().insertImage(fp);
        });
        dc3->addLayout(fmtGrid);

        wps_spacing_combo_ = new QComboBox(dCard3);
        wps_spacing_combo_->addItems({"行距 1.0x", "行距 1.15x", "行距 1.5x", "行距 2.0x"});
        wps_spacing_combo_->setCurrentIndex(2);
        dc3->addWidget(wps_spacing_combo_);

        connect(wps_style_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
            if (idx == 0) applyHeadingToWord(0);
            else if (idx >= 1 && idx <= 4) applyHeadingToWord(idx);
            else { QTextBlockFormat bf; bf.setLeftMargin(22); word_editor_->textCursor().mergeBlockFormat(bf); }
        });
        connect(wps_font_combo_, &QComboBox::currentTextChanged, this, [this](const QString& fn) {
            QTextCharFormat f; f.setFontFamilies({fn}); word_editor_->mergeCurrentCharFormat(f);
        });
        connect(wps_size_combo_, &QComboBox::currentTextChanged, this, [this](const QString& sz) {
            double pt = sz.toDouble();
            if (pt > 0) { QTextCharFormat f; f.setFontPointSize(pt); word_editor_->mergeCurrentCharFormat(f); }
        });
        connect(wps_spacing_combo_, &QComboBox::currentIndexChanged, this, [this](int idx) {
            static const double mults[4] = {100.0, 115.0, 150.0, 200.0};
            QTextBlockFormat bf;
            bf.setLineHeight(mults[std::clamp(idx, 0, 3)], QTextBlockFormat::ProportionalHeight);
            word_editor_->textCursor().mergeBlockFormat(bf);
        });
        dockL->addWidget(dCard3);
        dockL->addStretch();
        dockScroll->setWidget(dockInner);
        rightH->addWidget(dockScroll);

        // Right Stage Column: Modes + Bottom Status Bar
        auto* stageCol = new QVBoxLayout();
        stageCol->setContentsMargins(0, 0, 0, 0);
        stageCol->setSpacing(6);

        ed_mode_stack_ = new QStackedWidget();

        // Mode 0: Scaled A4 Paper Sheet in QScrollArea
        word_scroll_ = new QScrollArea();
        word_scroll_->setWidgetResizable(false);
        word_scroll_->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
        word_scroll_->viewport()->installEventFilter(this);

        word_scroll_content_ = new QWidget();
        word_scroll_content_->setStyleSheet(QStringLiteral("background: transparent;"));
        auto* scL = new QVBoxLayout(word_scroll_content_);
        scL->setContentsMargins(18, 16, 18, 24);
        scL->setAlignment(Qt::AlignHCenter | Qt::AlignTop);

        word_paper_frame_ = new A4PaperWidget(word_scroll_content_);
        word_paper_frame_->setStyleSheet(QStringLiteral("A4PaperWidget { background: #FFFFFF; border: 1px solid #CBD5E1; border-radius: 3px; }"));
        apply_card_shadow(word_paper_frame_, pal_.is_dark, 32, 6, 36);
        auto* pfL = new QVBoxLayout(word_paper_frame_);
        pfL->setContentsMargins(68, 36, 68, 36);
        pfL->setSpacing(6);

        word_ruler_widget_ = new A4RulerWidget(word_paper_frame_);
        pfL->addWidget(word_ruler_widget_);

        word_editor_ = new QTextEdit(word_paper_frame_);
        word_editor_->setFrameStyle(QFrame::NoFrame);
        word_editor_->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        word_editor_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        word_editor_->setStyleSheet(QStringLiteral("QTextEdit { background: #FFFFFF; color: #111827; border: none; }"));
        connect(word_editor_, &QTextEdit::textChanged, this, [this]() {
            if (doc_updating_) return;
            markDocDirty(true);
            syncWordMetrics();
        });
        pfL->addWidget(word_editor_, 1);

        word_footer_lbl_ = new QLabel(QStringLiteral("— 第 1 页 / 共 1 页 —"), word_paper_frame_);
        word_footer_lbl_->setAlignment(Qt::AlignCenter);
        word_footer_lbl_->setStyleSheet(QStringLiteral("color:#94A3B8;font-size:10.5px;border:none;border-top:1px solid #F1F5F9;padding-top:6px;"));
        pfL->addWidget(word_footer_lbl_);

        scL->addWidget(word_paper_frame_);
        word_scroll_->setWidget(word_scroll_content_);
        ed_mode_stack_->addWidget(word_scroll_);

        // Mode 1: Markdown Source
        auto* mdCard = makeCard();
        auto* mdL = new QVBoxLayout(mdCard);
        mdL->setContentsMargins(10, 10, 10, 10);
        md_editor_ = new QPlainTextEdit(mdCard);
        md_editor_->setObjectName(QStringLiteral("MdEditor"));
        QFont mdf("Consolas"); mdf.setPointSize(11);
        md_editor_->setFont(mdf);
        connect(md_editor_, &QPlainTextEdit::textChanged, this, [this]() {
            if (!doc_updating_) markDocDirty(true);
        });
        mdL->addWidget(md_editor_, 1);
        ed_mode_stack_->addWidget(mdCard);

        // Mode 2: LaTeX Studio
        auto* texCard = makeCard();
        auto* txL = new QVBoxLayout(texCard);
        txL->setContentsMargins(14, 14, 14, 14);
        txL->setSpacing(8);
        auto* txHead = new QHBoxLayout();
        txHead->addWidget(new QLabel(QStringLiteral("LaTeX 公式编辑与 Word OMML 转译"), texCard));
        auto* insTexBtn = new QPushButton(QStringLiteral("+ 插入为 $$ 公式块"), texCard);
        insTexBtn->setProperty("btn", "primary");
        connect(insTexBtn, &QPushButton::clicked, this, [this]() {
            QString t = tex_box_->toPlainText().trimmed();
            if (!t.isEmpty()) insertSnippetIntoDoc("$$\n" + t + "\n$$\n");
        });
        txHead->addStretch();
        txHead->addWidget(insTexBtn);
        txL->addLayout(txHead);
        tex_box_ = new QPlainTextEdit(texCard);
        tex_box_->setPlainText(QStringLiteral("\\mathcal{L}_{total} = \\sum_{i=1}^{N} (y_i - \\hat{y}_i)^2 + \\lambda \\|\\mathbf{W}\\|_2^2"));
        tex_box_->setMaximumHeight(120);
        txL->addWidget(tex_box_);
        txL->addWidget(new QLabel(QStringLiteral("当前文档中的公式列表："), texCard));
        tex_list_ = new QListWidget(texCard);
        txL->addWidget(tex_list_, 1);
        ed_mode_stack_->addWidget(texCard);

        stageCol->addWidget(ed_mode_stack_, 1);

        // Bottom Word Status / Page Bar
        auto* statusCard = makeCard(rightContainer);
        auto* sbL = new QHBoxLayout(statusCard);
        sbL->setContentsMargins(10, 5, 10, 5);
        sbL->setSpacing(6);
        auto* prevP = new QPushButton(QStringLiteral("◀ 上一页"), statusCard); prevP->setProperty("btn", "pill");
        word_page_spin_ = new QSpinBox(statusCard); word_page_spin_->setRange(1, 1); word_page_spin_->setFixedWidth(52);
        word_total_pages_lbl_ = new QLabel(QStringLiteral("/ 1 页"), statusCard);
        auto* nextP = new QPushButton(QStringLiteral("下一页 ▶"), statusCard); nextP->setProperty("btn", "pill");
        auto* brkP = new QPushButton(QStringLiteral("+ 分页"), statusCard); brkP->setProperty("btn", "pill");

        auto* szOut = new QPushButton(QStringLiteral("−"), statusCard); szOut->setProperty("btn", "pill");
        word_status_zoom_btn_ = new QPushButton(QStringLiteral("105%"), statusCard); word_status_zoom_btn_->setProperty("btn", "pill");
        auto* szIn = new QPushButton(QStringLiteral("+"), statusCard); szIn->setProperty("btn", "pill");
        word_char_lbl_ = new QLabel(QStringLiteral("0 字"), statusCard);
        word_char_lbl_->setProperty("muted", true);
        auto* a4Lbl = new QLabel(QStringLiteral("A4 (210×297mm)"), statusCard);
        a4Lbl->setProperty("muted", true);

        sbL->addWidget(prevP);
        sbL->addWidget(new QLabel(QStringLiteral("第"), statusCard));
        sbL->addWidget(word_page_spin_);
        sbL->addWidget(word_total_pages_lbl_);
        sbL->addWidget(nextP);
        sbL->addWidget(brkP);
        sbL->addStretch();
        sbL->addWidget(szOut);
        sbL->addWidget(word_status_zoom_btn_);
        sbL->addWidget(szIn);
        sbL->addWidget(word_char_lbl_);
        sbL->addWidget(a4Lbl);

        connect(prevP, &QPushButton::clicked, this, [this]() { goToWordPage(word_page_spin_->value() - 1); });
        connect(nextP, &QPushButton::clicked, this, [this]() { goToWordPage(word_page_spin_->value() + 1); });
        connect(word_page_spin_, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
            if (!doc_updating_) goToWordPage(v);
        });
        connect(brkP, &QPushButton::clicked, this, [this]() {
            QTextBlockFormat bf;
            bf.setPageBreakPolicy(QTextFormat::PageBreak_AlwaysBefore);
            word_editor_->textCursor().insertBlock(bf);
            syncWordMetrics();
        });
        connect(szOut, &QPushButton::clicked, this, [this]() { stepWordZoom(-0.10); });
        connect(szIn, &QPushButton::clicked, this, [this]() { stepWordZoom(0.10); });
        connect(word_status_zoom_btn_, &QPushButton::clicked, this, [this]() { setWordCustomZoom(1.0); });

        stageCol->addWidget(statusCard);
        rightH->addLayout(stageCol, 1);

        sp->addWidget(leftW);
        sp->addWidget(rightContainer);
        sp->setStretchFactor(0, 31);
        sp->setStretchFactor(1, 69);

        switchEditorModeQt("word");
        return page;
    }

    void updateWordStageBackdrop() {
        if (!word_scroll_) return;
        word_scroll_->setStyleSheet(pal_.is_dark
            ? QStringLiteral("QScrollArea { background: #141922; border-radius: 12px; border: 1px solid #2B3648; }")
            : QStringLiteral("QScrollArea { background: #E4E8F0; border-radius: 12px; border: 1px solid #CBD5E1; }"));
    }

    void updateWordPaperScale() {
        if (!word_scroll_ || !word_paper_frame_ || !word_editor_) return;
        int availW = std::max(380, word_scroll_->viewport()->width() - 44);
        int availH = std::max(340, word_scroll_->viewport()->height() - 38);
        double z = 1.0;
        if (word_fit_mode_ == "width") {
            z = std::clamp(availW / 794.0, 0.95, 1.48);
        } else if (word_fit_mode_ == "page") {
            z = std::clamp(std::min(availW / 794.0, availH / 1123.0), 0.55, 1.25);
        } else {
            z = std::clamp(word_custom_zoom_, 0.50, 1.80);
        }
        word_eff_zoom_ = z;

        int sheetW = static_cast<int>(std::round(794.0 * z));
        int minSheetH = static_cast<int>(std::round(1123.0 * z));
        int padX = static_cast<int>(std::round(64.0 * z));
        int padY = static_cast<int>(std::round(36.0 * z));
        word_paper_frame_->layout()->setContentsMargins(padX, padY, padX, padY);
        word_paper_frame_->setMarginsPx(padX, padY);
        word_paper_frame_->setFixedWidth(sheetW);
        word_paper_frame_->setMinimumHeight(minSheetH);

        // Clean proportional typography inside QTextEdit for razor-sharp native rendering
        QFont baseF(QStringLiteral("Times New Roman"));
        baseF.setPointSizeF(10.5 * z);
        word_editor_->setFont(baseF);
        syncWordMetrics();

        int pct = static_cast<int>(std::round(z * 100.0));
        doc_updating_ = true;
        if (word_zoom_pct_btn_) word_zoom_pct_btn_->setText(QString("%1%").arg(pct));
        if (word_status_zoom_btn_) word_status_zoom_btn_->setText(QString("%1%").arg(pct));
        if (word_zoom_slider_) word_zoom_slider_->setValue(std::clamp(pct, 50, 180));
        if (word_fit_width_btn_) word_fit_width_btn_->setChecked(word_fit_mode_ == "width");
        if (word_fit_page_btn_) word_fit_page_btn_->setChecked(word_fit_mode_ == "page");
        doc_updating_ = false;
    }

    void stepWordZoom(double delta) {
        setWordCustomZoom(std::round((word_eff_zoom_ + delta) * 20.0) / 20.0);
    }

    void setWordCustomZoom(double z) {
        word_custom_zoom_ = std::clamp(z, 0.50, 1.80);
        word_fit_mode_ = "custom";
        updateWordPaperScale();
    }

    void syncWordMetrics() {
        if (!word_editor_ || !word_paper_frame_) return;
        QTextDocument* doc = word_editor_->document();
        int edW = std::max(200, word_paper_frame_->width() - 130);
        doc->setTextWidth(edW);
        int docH = static_cast<int>(std::ceil(doc->size().height()));
        word_editor_->setMinimumHeight(std::max(static_cast<int>(940 * word_eff_zoom_), docH + 40));
        int pageBodyH = std::max(400, static_cast<int>(980 * word_eff_zoom_));
        word_total_pages_ = std::max(1, static_cast<int>(std::ceil(static_cast<double>(docH) / pageBodyH)));
        int curP = 1;
        if (word_scroll_) {
            int st = word_scroll_->verticalScrollBar()->value();
            curP = std::clamp(st / pageBodyH + 1, 1, word_total_pages_);
        }
        doc_updating_ = true;
        if (word_page_spin_) { word_page_spin_->setMaximum(word_total_pages_); word_page_spin_->setValue(curP); }
        doc_updating_ = false;
        if (word_total_pages_lbl_) word_total_pages_lbl_->setText(QString("/ %1 页").arg(word_total_pages_));
        if (word_footer_lbl_) word_footer_lbl_->setText(QString("— 第 %1 页 / 共 %2 页 —").arg(curP).arg(word_total_pages_));
        if (word_char_lbl_) {
            QString txt = doc->toPlainText().simplified();
            txt.remove(' ');
            word_char_lbl_->setText(QString("%1 字").arg(txt.size()));
        }
        word_paper_frame_->adjustSize();
        if (word_scroll_content_) word_scroll_content_->adjustSize();
    }

    void goToWordPage(int page) {
        if (!word_scroll_) return;
        int target = std::clamp(page, 1, word_total_pages_);
        int pageBodyH = static_cast<int>(980 * word_eff_zoom_);
        word_scroll_->verticalScrollBar()->setValue((target - 1) * pageBodyH);
        if (word_footer_lbl_) word_footer_lbl_->setText(QString("— 第 %1 页 / 共 %2 页 —").arg(target).arg(word_total_pages_));
    }

    void toggleWordCharFormat(const QString& kind) {
        QTextCharFormat f = word_editor_->currentCharFormat();
        if (kind == "bold") f.setFontWeight(f.fontWeight() >= QFont::Bold ? QFont::Normal : QFont::Bold);
        else if (kind == "italic") f.setFontItalic(!f.fontItalic());
        else if (kind == "underline") f.setFontUnderline(!f.fontUnderline());
        else if (kind == "strike") f.setFontStrikeOut(!f.fontStrikeOut());
        else if (kind == "code") {
            bool isC = f.fontFixedPitch();
            f.setFontFixedPitch(!isC);
            f.setFontFamilies({!isC ? QStringLiteral("Consolas") : QStringLiteral("宋体")});
            if (!isC) f.setBackground(QColor("#F1F5F9")); else f.clearBackground();
        }
        word_editor_->mergeCurrentCharFormat(f);
    }

    void applyHeadingToWord(int lvl) {
        QTextCursor cur = word_editor_->textCursor();
        QTextBlockFormat bf;
        bf.setHeadingLevel(lvl);
        QTextCharFormat cf;
        cf.setFontFamilies({lvl > 0 ? QStringLiteral("黑体") : QStringLiteral("宋体")});
        static const double pts[5] = {10.5, 22.0, 16.0, 14.0, 12.0};
        cf.setFontPointSize(pts[std::clamp(lvl, 0, 4)] * word_eff_zoom_);
        cf.setFontWeight(lvl > 0 ? QFont::Bold : QFont::Normal);
        cur.mergeBlockFormat(bf);
        cur.mergeCharFormat(cf);
    }

    void switchEditorModeQt(const std::string& mode) {
        if (cur_ed_mode_ == "word" && mode != "word") {
            cur_doc_markdown_ = qt_document_to_markdown(word_editor_->document());
        } else if (cur_ed_mode_ == "md" && mode != "md") {
            cur_doc_markdown_ = md_editor_->toPlainText().toStdString();
        }
        cur_ed_mode_ = mode;
        int idx = (mode == "word" ? 0 : (mode == "md" ? 1 : 2));
        for (int i = 0; i < 3; ++i) {
            if (ed_mode_btns_[i]) ed_mode_btns_[i]->setChecked(i == idx);
        }
        ed_mode_stack_->setCurrentIndex(idx);
        doc_updating_ = true;
        if (mode == "word") {
            word_editor_->setHtml(render_markdown_for_word_editor(cur_doc_markdown_, active_doc_path_));
            updateWordPaperScale();
        } else if (mode == "md") {
            md_editor_->setPlainText(QString::fromStdString(cur_doc_markdown_));
        } else {
            tex_list_->clear();
            for (const auto& f : extract_latex_blocks(cur_doc_markdown_)) {
                tex_list_->addItem(QString::fromStdString(f));
            }
        }
        doc_updating_ = false;
    }

    void markDocDirty(bool d) {
        doc_dirty_ = d;
        if (doc_dirty_lbl_) doc_dirty_lbl_->setVisible(d);
    }

    void insertSnippetIntoDoc(const QString& sn) {
        if (cur_ed_mode_ == "md") {
            md_editor_->insertPlainText(sn);
        } else if (cur_ed_mode_ == "word") {
            word_editor_->textCursor().insertText(sn);
        }
        if (QApplication::clipboard()) QApplication::clipboard()->setText(sn);
        showToast(QStringLiteral("已插入语法并复制"));
    }

    void setupVaultWatcher() {
        vault_watcher_ = new QFileSystemWatcher(this);
        connect(vault_watcher_, &QFileSystemWatcher::directoryChanged, this, [this]() {
            refreshVaultTree();
        });
    }

    void refreshVaultTree() {
        if (!vault_arc_) return;
        std::string r = vault_.root();
        if (vault_path_lbl_) {
            vault_path_lbl_->setText(vault_.root_exists() ? QString("📁 %1").arg(QString::fromStdString(r)) : QStringLiteral("📁 未选择"));
            vault_path_lbl_->setToolTip(QString::fromStdString(r));
        }
        JsonValue tree = vault_.scan_tree();
        vault_arc_->setTreeData(tree);
        if (vault_watcher_ && vault_.root_exists()) {
            QString qr = QString::fromStdString(r);
            if (!vault_watcher_->directories().contains(qr)) vault_watcher_->addPath(qr);
        }
    }

    void pickVaultFolderQt() {
        QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择本地知识库根文件夹"), QString::fromStdString(vault_.root()));
        if (dir.isEmpty()) return;
        std::string utf = dir.toStdString();
        vault_.set_root(utf);
        cfg_.mutable_get().vault_path = utf;
        cfg_.save();
        active_doc_path_.clear();
        doc_name_lbl_->setText(QStringLiteral("未打开文档"));
        refreshVaultTree();
        showToast(QStringLiteral("已切换本地知识库 ✓"));
    }

    void createNewFolderQt() {
        if (!vault_.root_exists()) { pickVaultFolderQt(); if (!vault_.root_exists()) return; }
        bool ok = false;
        QString name = QInputDialog::getText(this, QStringLiteral("新建目录"), QStringLiteral("目录名称（相对知识库根目录）："), QLineEdit::Normal, QString(), &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        std::string err;
        if (vault_.create_folder(name.trimmed().toStdString(), &err)) {
            refreshVaultTree();
            vault_arc_->ensureDirExpanded(name.trimmed().toStdString());
            showToast(QStringLiteral("目录已创建 ✓"));
        } else QMessageBox::warning(this, QStringLiteral("创建失败"), QString::fromStdString(err));
    }

    void createNewNoteQt() {
        if (!vault_.root_exists()) { pickVaultFolderQt(); if (!vault_.root_exists()) return; }
        bool ok = false;
        QString defName = QString("新笔记_%1.md").arg(QDate::currentDate().toString("yyyy-MM-dd"));
        QString name = QInputDialog::getText(this, QStringLiteral("新建笔记"), QStringLiteral("笔记路径（可含目录，如 算法/笔记.md）："), QLineEdit::Normal, defName, &ok);
        if (!ok || name.trimmed().isEmpty()) return;
        std::string rel = name.trimmed().toStdString();
        std::string initMd = "# " + fs::path(rel).stem().string() + "\n\n在此开始编写 Markdown、表格与 $$LaTeX$$ 公式。\n";
        std::string err;
        if (vault_.create_note(rel, initMd, &err)) {
            refreshVaultTree();
            openVaultDoc(rel);
        } else QMessageBox::warning(this, QStringLiteral("新建失败"), QString::fromStdString(err));
    }

    void openVaultDoc(const std::string& relPath) {
        VaultFileContent fc;
        std::string err;
        if (!vault_.open_document(relPath, fc, &err)) {
            QMessageBox::warning(this, QStringLiteral("打开失败"), QString::fromStdString(err));
            return;
        }
        active_doc_path_ = fc.rel_path;
        cur_doc_markdown_ = fc.markdown;
        if (doc_name_lbl_) doc_name_lbl_->setText(QString::fromStdString(fc.rel_path));
        if (vault_arc_) vault_arc_->setActivePath(fc.rel_path);
        doc_updating_ = true;
        word_editor_->setHtml(render_markdown_for_word_editor(cur_doc_markdown_, active_doc_path_));
        md_editor_->setPlainText(QString::fromStdString(cur_doc_markdown_));
        doc_updating_ = false;
        markDocDirty(false);
        updateWordPaperScale();
    }

    bool saveActiveDocQt() {
        if (active_doc_path_.empty()) {
            createNewNoteQt();
            return !active_doc_path_.empty();
        }
        if (cur_ed_mode_ == "word") cur_doc_markdown_ = qt_document_to_markdown(word_editor_->document());
        else if (cur_ed_mode_ == "md") cur_doc_markdown_ = md_editor_->toPlainText().toStdString();
        std::string err;
        if (vault_.save_document(active_doc_path_, cur_doc_markdown_, &err)) {
            markDocDirty(false);
            showToast(QStringLiteral("文档已保存 ✓"));
            return true;
        }
        QMessageBox::warning(this, QStringLiteral("保存失败"), QString::fromStdString(err));
        return false;
    }

    void exportActiveDocQt(const std::string& fmt, bool pickOut) {
        if (cur_ed_mode_ == "word") cur_doc_markdown_ = qt_document_to_markdown(word_editor_->document());
        else if (cur_ed_mode_ == "md") cur_doc_markdown_ = md_editor_->toPlainText().toStdString();
        std::string outPath;
        if (pickOut) {
            QString fp = QFileDialog::getSaveFileName(this, QStringLiteral("另存为"), QString(), QString("*.%1").arg(QString::fromStdString(fmt)));
            if (fp.isEmpty()) return;
            outPath = fp.toStdString();
        }
        std::string err;
        if (vault_.convert_document(active_doc_path_, cur_doc_markdown_, fmt, outPath, &err)) {
            refreshVaultTree();
            showToast(QString("导出 %1 成功 ✓").arg(QString::fromStdString(fmt).toUpper()));
        } else {
            QMessageBox::warning(this, QStringLiteral("导出失败"), QString::fromStdString(err));
        }
    }

    // ========================================================================
    // PAGE 2: C/C++ Data Structures & Algorithms (Ch 0-13 + Practice IDE)
    // ========================================================================
    QWidget* buildDsPage() {
        auto* page = new QWidget();
        auto* sp = new QSplitter(Qt::Horizontal, page);
        auto* lay = new QHBoxLayout(page);
        lay->setContentsMargins(10, 10, 10, 10);
        lay->addWidget(sp);

        // Left column: Chapter Tree + Visualizer
        auto* leftW = new QWidget();
        auto* leftL = new QVBoxLayout(leftW);
        leftL->setContentsMargins(0, 0, 0, 0);
        leftL->setSpacing(8);

        auto* chapCard = makeCard(leftW);
        auto* ccL = new QVBoxLayout(chapCard);
        ccL->setContentsMargins(8, 8, 8, 8);
        ccL->setSpacing(6);

        auto* cHead = new QHBoxLayout();
        auto* ctLbl = new QLabel(QStringLiteral("📚 C/C++ 算法与数据结构课程库"), chapCard);
        ctLbl->setStyleSheet(QStringLiteral("font-weight:700;font-size:12px;"));
        ds_prog_pill_ = new QLabel(QStringLiteral("已练 0 / 76 题"), chapCard);
        ds_prog_pill_->setProperty("pill", "green");
        ds_db_pill_ = new QLabel(chapCard);
        ds_db_pill_->setProperty("pill", "blue");
        bool isSql = (ds_.get_catalog("", "").get("db_mode").str_val == "sqlite3");
        ds_db_pill_->setText(QString("%1 · %2").arg(
            isSql ? QStringLiteral("SQLite3") : QStringLiteral("课程库"),
            DsManager::find_cpp_compiler().empty() ? QStringLiteral("无g++") : QStringLiteral("g++")));
        cHead->addWidget(ctLbl);
        cHead->addWidget(ds_prog_pill_);
        cHead->addStretch();
        cHead->addWidget(ds_db_pill_);
        ccL->addLayout(cHead);

        ds_search_inp_ = new QLineEdit(chapCard);
        ds_search_inp_->setPlaceholderText(QStringLiteral("🔍 搜索章节、C基础、Dijkstra、KMP、AVL..."));
        ccL->addWidget(ds_search_inp_);

        auto* catRow = new QHBoxLayout();
        catRow->setSpacing(4);
        const char* catIds[6] = {
            "全部", "C语言入门(Ch0-3)", "算法与数学(Ch4-5)",
            "C++ STL(Ch6)", "数据结构与搜索(Ch7-9)", "图论/DP/高阶(Ch10-13)"
        };
        const char* catLabels[6] = {
            "全部", "C入门", "算法数学", "STL容器", "线性/树", "图论/DP"
        };
        for (int i = 0; i < 6; ++i) {
            auto* cb = new QPushButton(QString::fromUtf8(catLabels[i]), chapCard);
            cb->setProperty("btn", "seg");
            cb->setCheckable(true);
            cb->setChecked(i == 0);
            connect(cb, &QPushButton::clicked, this, [this, i, cat = std::string(catIds[i])]() {
                ds_cat_filter_ = (i == 0 ? "" : cat);
                for (int k = 0; k < static_cast<int>(ds_cat_btns_.size()); ++k) {
                    ds_cat_btns_[k]->setChecked(k == i);
                }
                refreshDsCatalog(false);
            });
            ds_cat_btns_.push_back(cb);
            catRow->addWidget(cb);
        }
        ccL->addLayout(catRow);

        ds_tree_ = new QTreeWidget(chapCard);
        ds_tree_->setHeaderHidden(true);
        ds_tree_->setIndentation(0);
        ds_tree_->setRootIsDecorated(false);
        ds_tree_->setAnimated(false);
        ds_tree_->setMouseTracking(true);
        ds_cat_delegate_ = new DsCatalogDelegate(ds_tree_);
        ds_tree_->setItemDelegate(ds_cat_delegate_);
        ccL->addWidget(ds_tree_, 1);
        leftL->addWidget(chapCard, 6);

        connect(ds_search_inp_, &QLineEdit::textChanged, this, [this]() { refreshDsCatalog(false); });
        connect(ds_tree_, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item, int) {
            if (!item) return;
            QString lid = item->data(0, Qt::UserRole).toString();
            if (!lid.isEmpty()) selectDsLesson(lid.toStdString());
            else item->setExpanded(!item->isExpanded());
        });

        // Visualizer Card
        auto* vizCard = makeCard(leftW);
        auto* vcL = new QVBoxLayout(vizCard);
        vcL->setContentsMargins(10, 9, 10, 9);
        vcL->setSpacing(6);
        auto* vHead = new QHBoxLayout();
        ds_viz_title_lbl_ = new QLabel(QStringLiteral("🎯 算法结构动态演练"), vizCard);
        ds_viz_title_lbl_->setStyleSheet(QStringLiteral("font-weight:700;font-size:12.5px;"));
        ds_viz_step_lbl_ = new QLabel(QStringLiteral("Step 1 / 4"), vizCard);
        ds_viz_step_lbl_->setProperty("pill", "blue");
        vHead->addWidget(ds_viz_title_lbl_, 1);
        vHead->addWidget(ds_viz_step_lbl_);
        vcL->addLayout(vHead);

        ds_viz_ = new DsVisualizerWidget(vizCard);
        vcL->addWidget(ds_viz_, 1);

        auto* vCtl = new QHBoxLayout();
        auto* vPrev = new QPushButton(QStringLiteral("⏮ 上一步"), vizCard); vPrev->setProperty("btn", "pill");
        ds_viz_play_btn_ = new QPushButton(QStringLiteral("⏯ 自动演示"), vizCard); ds_viz_play_btn_->setProperty("btn", "primary");
        auto* vNext = new QPushButton(QStringLiteral("⏭ 下一步"), vizCard); vNext->setProperty("btn", "pill");
        ds_viz_cap_ = new QLabel(vizCard);
        ds_viz_cap_->setProperty("muted", true);
        ds_viz_cap_->setStyleSheet(QStringLiteral("font-size:11px;"));
        vCtl->addWidget(vPrev); vCtl->addWidget(ds_viz_play_btn_); vCtl->addWidget(vNext);
        vCtl->addWidget(ds_viz_cap_, 1);
        vcL->addLayout(vCtl);
        leftL->addWidget(vizCard, 4);

        ds_viz_timer_ = new QTimer(this);
        connect(ds_viz_timer_, &QTimer::timeout, this, [this]() {
            ds_viz_->setStep(ds_viz_->step() + 1);
            syncDsVizLabels();
        });
        connect(vPrev, &QPushButton::clicked, this, [this]() { ds_viz_->setStep(ds_viz_->step() - 1); syncDsVizLabels(); });
        connect(vNext, &QPushButton::clicked, this, [this]() { ds_viz_->setStep(ds_viz_->step() + 1); syncDsVizLabels(); });
        connect(ds_viz_play_btn_, &QPushButton::clicked, this, [this]() {
            if (ds_viz_timer_->isActive()) { ds_viz_timer_->stop(); ds_viz_play_btn_->setText(QStringLiteral("⏯ 自动演示")); }
            else { ds_viz_timer_->start(1250); ds_viz_play_btn_->setText(QStringLiteral("⏸ 暂停")); }
        });

        // Right Column: Vertical Splitter (Top Tutorial+Template, Bottom Practice IDE)
        auto* rightSp = new QSplitter(Qt::Vertical);
        rightSp->setChildrenCollapsible(false);

        // Top Tutorial Card
        auto* tutCard = makeCard();
        auto* tcL = new QVBoxLayout(tutCard);
        tcL->setContentsMargins(10, 8, 10, 8);
        tcL->setSpacing(6);

        auto* tHead = new QHBoxLayout();
        ds_les_title_ = new QLabel(QStringLiteral("加载教程中..."), tutCard);
        ds_les_title_->setStyleSheet(QStringLiteral("font-weight:700;font-size:13px;"));
        ds_les_time_ = new QLabel(tutCard); ds_les_time_->setProperty("pill", "blue");
        ds_les_space_ = new QLabel(tutCard); ds_les_space_->setProperty("pill", "green");
        ds_les_src_ = new QLabel(tutCard); ds_les_src_->setProperty("pill", "purple");
        ds_mastered_btn_ = new QPushButton(QStringLiteral("○ 标为已掌握"), tutCard); ds_mastered_btn_->setProperty("btn", "pill");
        auto* noteTgBtn = new QPushButton(QStringLiteral("📒 笔记"), tutCard); noteTgBtn->setProperty("btn", "pill");
        auto* pushVaultBtn = new QPushButton(QStringLiteral("✎ 存入知识库"), tutCard); pushVaultBtn->setProperty("btn", "pill");

        tHead->addWidget(ds_les_title_);
        tHead->addWidget(ds_les_time_);
        tHead->addWidget(ds_les_space_);
        tHead->addStretch();
        tHead->addWidget(ds_les_src_);
        tHead->addWidget(ds_mastered_btn_);
        tHead->addWidget(noteTgBtn);
        tHead->addWidget(pushVaultBtn);
        tcL->addLayout(tHead);

        ds_note_drawer_ = new QFrame(tutCard);
        ds_note_drawer_->setVisible(false);
        auto* ndL = new QHBoxLayout(ds_note_drawer_);
        ndL->setContentsMargins(0, 2, 0, 2);
        ds_note_inp_ = new QLineEdit(ds_note_drawer_);
        ds_note_inp_->setPlaceholderText(QStringLiteral("记录本节错题反思、边界坑点（持久化存入 SQLite3）..."));
        auto* saveNdBtn = new QPushButton(QStringLiteral("保存笔记"), ds_note_drawer_);
        saveNdBtn->setProperty("btn", "primary");
        ndL->addWidget(ds_note_inp_, 1);
        ndL->addWidget(saveNdBtn);
        tcL->addWidget(ds_note_drawer_);

        connect(noteTgBtn, &QPushButton::clicked, this, [this]() {
            ds_note_drawer_->setVisible(!ds_note_drawer_->isVisible());
        });
        connect(saveNdBtn, &QPushButton::clicked, this, [this]() {
            if (cur_ds_lesson_.id.empty()) return;
            cur_ds_lesson_.note = ds_note_inp_->text().toStdString();
            ds_.save_progress(cur_ds_lesson_.id, cur_ds_lesson_.mastered, cur_ds_lesson_.note);
            showToast(QStringLiteral("已保存本节学习笔记 ✓"));
        });
        connect(ds_mastered_btn_, &QPushButton::clicked, this, [this]() {
            if (cur_ds_lesson_.id.empty()) return;
            cur_ds_lesson_.mastered = !cur_ds_lesson_.mastered;
            ds_.save_progress(cur_ds_lesson_.id, cur_ds_lesson_.mastered, cur_ds_lesson_.note);
            updateDsMasteredBtn();
            refreshDsCatalog(false);
            showToast(cur_ds_lesson_.mastered ? QStringLiteral("已标为「已掌握」✓") : QStringLiteral("已取消掌握标记"));
        });
        connect(pushVaultBtn, &QPushButton::clicked, this, [this]() {
            if (cur_ds_lesson_.id.empty()) return;
            if (!vault_.root_exists()) { showToast(QStringLiteral("请先在「知识」选择知识库目录")); return; }
            std::string rel = "数据结构笔记/" + cur_ds_lesson_.id + ".md";
            std::string md = "# " + cur_ds_lesson_.title + "\n\n> 来源：" + cur_ds_lesson_.source_ref +
                             "\n\n```cpp\n" + cur_ds_lesson_.code_template + "\n```\n";
            std::string err;
            if (vault_.create_note(rel, md, &err) || vault_.save_document(rel, md, &err)) {
                refreshVaultTree();
                showToast(QString("✎ 已存入知识库：%1").arg(QString::fromStdString(rel)));
            }
        });

        auto* tutSp = new QSplitter(Qt::Horizontal, tutCard);
        tutSp->setChildrenCollapsible(false);
        ds_tut_widget_ = new DsTutorialWidget(tutSp);
        ds_tut_widget_->setMinimumWidth(280);

        auto* tmplBox = new QFrame(tutSp);
        tmplBox->setMinimumWidth(260);
        tmplBox->setStyleSheet(QStringLiteral("QFrame { background: #14171C; border: 1px solid #262B35; border-radius: 10px; }"));
        auto* tbL = new QVBoxLayout(tmplBox);
        tbL->setContentsMargins(0, 0, 0, 0);
        tbL->setSpacing(0);
        auto* tbHead = new QWidget(tmplBox);
        tbHead->setStyleSheet(QStringLiteral("background:#101216;border-bottom:1px solid #262B35;border-top-left-radius:9px;border-top-right-radius:9px;"));
        auto* tbhL = new QHBoxLayout(tbHead);
        tbhL->setContentsMargins(10, 6, 8, 6);
        auto* tmplTitle = new QLabel(QStringLiteral("核心源码模板 (C / C++17)"), tbHead);
        tmplTitle->setStyleSheet(QStringLiteral("color:#E5E7EB;font-family:Consolas;font-size:11.5px;font-weight:600;"));
        auto* copyBtn = new QPushButton(QStringLiteral("📋 复制"), tbHead); copyBtn->setProperty("btn", "pill");
        auto* loadFreeBtn = new QPushButton(QStringLiteral("⬇ 载入到练习框"), tbHead); loadFreeBtn->setProperty("btn", "blue");
        tbhL->addWidget(tmplTitle); tbhL->addStretch(); tbhL->addWidget(copyBtn); tbhL->addWidget(loadFreeBtn);
        tbL->addWidget(tbHead);
        ds_tmpl_editor_ = new CodeEditor(tmplBox);
        ds_tmpl_editor_->setReadOnly(true);
        tbL->addWidget(ds_tmpl_editor_, 1);

        connect(copyBtn, &QPushButton::clicked, this, [this]() {
            if (QApplication::clipboard()) QApplication::clipboard()->setText(QString::fromStdString(cur_ds_lesson_.code_template));
            showToast(QStringLiteral("已复制核心代码模板"));
        });
        connect(loadFreeBtn, &QPushButton::clicked, this, [this]() {
            ds_free_code_ = QString::fromStdString(cur_ds_lesson_.code_template);
            selectDsExercise("free");
        });

        tutSp->addWidget(ds_tut_widget_);
        tutSp->addWidget(tmplBox);
        tutSp->setStretchFactor(0, 52);
        tutSp->setStretchFactor(1, 48);
        tutSp->setSizes({460, 420});
        tcL->addWidget(tutSp, 1);
        rightSp->addWidget(tutCard);

        // Bottom Practice IDE Card
        auto* pracCard = makeCard();
        pracCard->setObjectName(QStringLiteral("DsPracticeCard"));
        auto* pcL = new QVBoxLayout(pracCard);
        pcL->setContentsMargins(10, 9, 10, 9);
        pcL->setSpacing(6);

        auto* exHeaderRow = new QHBoxLayout();
        auto* exTabsHost = new QWidget(pracCard);
        ds_ex_tabs_layout_ = new QHBoxLayout(exTabsHost);
        ds_ex_tabs_layout_->setContentsMargins(0, 0, 0, 0);
        ds_ex_tabs_layout_->setSpacing(4);
        ds_ex_mode_pill_ = new QLabel(QStringLiteral("模式：核心填空补全"), pracCard);
        ds_ex_mode_pill_->setProperty("pill", "blue");
        ds_ex_status_pill_ = new QLabel(QStringLiteral("待提交评测"), pracCard);
        ds_ex_status_pill_->setProperty("pill", "amber");
        exHeaderRow->addWidget(exTabsHost);
        exHeaderRow->addStretch();
        exHeaderRow->addWidget(ds_ex_mode_pill_);
        exHeaderRow->addWidget(ds_ex_status_pill_);
        pcL->addLayout(exHeaderRow);

        auto* promptRow = new QHBoxLayout();
        ds_ex_prompt_lbl_ = new QLabel(pracCard);
        ds_ex_prompt_lbl_->setWordWrap(true);
        ds_ex_prompt_lbl_->setStyleSheet(QStringLiteral("font-size:12px;"));
        ds_ex_sample_lbl_ = new QLabel(pracCard);
        ds_ex_sample_lbl_->setProperty("muted", true);
        ds_ex_sample_lbl_->setStyleSheet(QStringLiteral("font-family:Consolas;font-size:11px;"));
        promptRow->addWidget(ds_ex_prompt_lbl_, 1);
        promptRow->addWidget(ds_ex_sample_lbl_);
        pcL->addLayout(promptRow);

        auto* ideSp = new QSplitter(Qt::Vertical, pracCard);
        auto* edFrame = new QFrame(ideSp);
        edFrame->setStyleSheet(QStringLiteral("QFrame { background:#14171C; border:1px solid #262B35; border-radius:8px; }"));
        auto* efL = new QVBoxLayout(edFrame);
        efL->setContentsMargins(0, 0, 0, 0);
        ds_ide_editor_ = new CodeEditor(edFrame);
        efL->addWidget(ds_ide_editor_);

        auto* conBox = new QWidget(ideSp);
        auto* cbL = new QVBoxLayout(conBox);
        cbL->setContentsMargins(0, 4, 0, 0);
        cbL->setSpacing(4);

        auto* actStrip = new QHBoxLayout();
        ds_run_btn_ = new QPushButton(QStringLiteral("▶ 编译运行 (自定义输入)"), conBox); ds_run_btn_->setProperty("btn", "blue");
        ds_submit_btn_ = new QPushButton(QStringLiteral("✓ 提交自测用例"), conBox); ds_submit_btn_->setProperty("btn", "green");
        auto* solBtn = new QPushButton(QStringLiteral("💡 填入标准解答"), conBox); solBtn->setProperty("btn", "pill");
        auto* rstBtn = new QPushButton(QStringLiteral("↺ 重置代码"), conBox); rstBtn->setProperty("btn", "pill");
        ds_stdin_inp_ = new QLineEdit(conBox);
        ds_stdin_inp_->setPlaceholderText(QStringLiteral("自定义 stdin 输入（可用 \\n 表示换行）..."));

        actStrip->addWidget(ds_run_btn_);
        actStrip->addWidget(ds_submit_btn_);
        actStrip->addWidget(solBtn);
        actStrip->addWidget(rstBtn);
        actStrip->addWidget(new QLabel(QStringLiteral("输入:"), conBox));
        actStrip->addWidget(ds_stdin_inp_, 1);
        cbL->addLayout(actStrip);

        ds_console_ = new QTextBrowser(conBox);
        ds_console_->setMinimumHeight(68);
        cbL->addWidget(ds_console_, 1);

        ideSp->addWidget(edFrame);
        ideSp->addWidget(conBox);
        ideSp->setStretchFactor(0, 62);
        ideSp->setStretchFactor(1, 38);
        pcL->addWidget(ideSp, 1);

        ds_ide_editor_->onCtrlEnter = [this]() { runDsExerciseAsync(cur_ex_id_ != "free"); };
        ds_ide_editor_->onTextEditedByUser = [this]() {
            if (cur_ex_id_ == "free") ds_free_code_ = ds_ide_editor_->toPlainText();
            else {
                for (auto& ex : cur_ds_lesson_.exercises) {
                    if (ex.id == cur_ex_id_) ex.user_code = ds_ide_editor_->toPlainText().toStdString();
                }
            }
        };
        connect(ds_run_btn_, &QPushButton::clicked, this, [this]() { runDsExerciseAsync(false); });
        connect(ds_submit_btn_, &QPushButton::clicked, this, [this]() { runDsExerciseAsync(true); });
        connect(solBtn, &QPushButton::clicked, this, [this]() {
            for (auto& ex : cur_ds_lesson_.exercises) {
                if (ex.id == cur_ex_id_) {
                    ex.user_code = ex.solution_code;
                    ds_ide_editor_->setCodeSafely(QString::fromStdString(ex.solution_code));
                    showToast(QStringLiteral("已填入参考解答，可直接按「✓ 提交自测用例」"));
                    return;
                }
            }
        });
        connect(rstBtn, &QPushButton::clicked, this, [this]() {
            if (cur_ex_id_ == "free") {
                ds_free_code_ = QStringLiteral("#include <iostream>\nusing namespace std;\n\nint main() {\n    return 0;\n}\n");
                ds_ide_editor_->setCodeSafely(ds_free_code_);
            } else {
                for (auto& ex : cur_ds_lesson_.exercises) {
                    if (ex.id == cur_ex_id_) {
                        ex.user_code = ex.starter_code;
                        ds_ide_editor_->setCodeSafely(QString::fromStdString(ex.starter_code));
                        break;
                    }
                }
            }
            showToast(QStringLiteral("已重置代码"));
        });

        rightSp->addWidget(pracCard);
        rightSp->setStretchFactor(0, 46);
        rightSp->setStretchFactor(1, 54);

        sp->setChildrenCollapsible(false);
        sp->addWidget(leftW);
        sp->addWidget(rightSp);
        sp->setStretchFactor(0, 38);
        sp->setStretchFactor(1, 62);
        sp->setSizes({520, 880});
        return page;
    }

    void syncDsVizLabels() {
        if (!ds_viz_) return;
        if (ds_viz_step_lbl_) ds_viz_step_lbl_->setText(QString("Step %1 / 4").arg(ds_viz_->step() + 1));
        if (ds_viz_cap_) ds_viz_cap_->setText(ds_viz_->captionText());
    }

    void refreshDsCatalog(bool autoSelectFirst) {
        std::string q = ds_search_inp_ ? ds_search_inp_->text().trimmed().toStdString() : "";
        JsonValue cat;
        {
            std::lock_guard<std::mutex> lk(ds_mutex_);
            cat = ds_.get_catalog(q, ds_cat_filter_);
        }
        int done = static_cast<int>(cat.get("completed_exercises").num_val);
        int tot = static_cast<int>(cat.get("total_exercises").num_val);
        if (ds_prog_pill_) ds_prog_pill_->setText(QString("已练 %1 / %2 题").arg(done).arg(tot));

        int savedScroll = ds_tree_->verticalScrollBar() ? ds_tree_->verticalScrollBar()->value() : 0;
        ds_tree_->clear();
        QString firstId;
        QTreeWidgetItem* preferredItem = nullptr;
        const auto& chaps = cat.get("chapters").arr_val;
        for (const auto& ch : chaps) {
            std::string cid = ch.get("id").str_val;
            const auto& lesArr = ch.get("lessons").arr_val;
            QString chTitle = QString::fromStdString(ch.get("title").str_val);
            QString chBadge = QString("%1 节 · %2/%3 题")
                .arg(static_cast<int>(lesArr.size()))
                .arg(static_cast<int>(ch.get("exercise_done").num_val))
                .arg(static_cast<int>(ch.get("exercise_total").num_val));
            auto* chItem = new QTreeWidgetItem(ds_tree_, QStringList(chTitle));
            chItem->setData(0, Qt::UserRole + 1, chBadge);

            bool expandChap = !q.empty() || cid == "ch0" || cid == "ch2" || cid == "ch7" ||
                              (!cur_ds_lesson_.chapter_id.empty() && cur_ds_lesson_.chapter_id == cid);
            for (const auto& l : lesArr) {
                QString lid = QString::fromStdString(l.get("id").str_val);
                if (firstId.isEmpty()) firstId = lid;
                bool mast = l.get("mastered").bool_val;
                int edone = static_cast<int>(l.get("exercise_done").num_val);
                int etot = static_cast<int>(l.get("exercise_total").num_val);
                QString badge = mast ? QStringLiteral("✓ 已掌握")
                                     : (etot > 0 ? QString("%1/%2 题").arg(edone).arg(etot) : QString());
                bool isDone = mast || (etot > 0 && edone >= etot);
                auto* li = new QTreeWidgetItem(chItem, QStringList(QString::fromStdString(l.get("title").str_val)));
                li->setData(0, Qt::UserRole, lid);
                li->setData(0, Qt::UserRole + 1, badge);
                li->setData(0, Qt::UserRole + 2, isDone);
                if (lid == QStringLiteral("2.1")) preferredItem = li;
                if (cur_ds_lesson_.id == lid.toStdString()) {
                    ds_tree_->setCurrentItem(li);
                    expandChap = true;
                }
            }
            chItem->setExpanded(expandChap);
        }
        if (ds_tree_->verticalScrollBar()) ds_tree_->verticalScrollBar()->setValue(savedScroll);
        if (!autoSelectFirst && ds_tree_->currentItem()) {
            ds_tree_->scrollToItem(ds_tree_->currentItem(), QAbstractItemView::EnsureVisible);
        }
        if (autoSelectFirst && cur_ds_lesson_.id.empty()) {
            QString target = preferredItem ? QStringLiteral("2.1") : firstId;
            if (!target.isEmpty()) selectDsLesson(target.toStdString());
        }
    }

    void updateDsMasteredBtn() {
        if (!ds_mastered_btn_) return;
        ds_mastered_btn_->setText(cur_ds_lesson_.mastered ? QStringLiteral("✓ 已掌握") : QStringLiteral("○ 标为已掌握"));
    }

public:
    void prepareSampleDocForScreenshot() {
        if (!word_editor_ || !active_doc_path_.empty()) return;
        std::string sampleMd =
            "# C++ 数据结构与算法学习笔记\n\n"
            "在此开始编写 Markdown、表格与 $$LaTeX$$ 公式。\n\n"
            "## 1. 复杂度分析口诀\n\n"
            "- 单次提交 **1 秒** 约执行 `10^8` 次基本操作。\n"
            "- 逆序高精度加法遵循 `t = a[i] + b[i] + carry`。\n\n"
            "| 算法 | 时间复杂度 | 适用数据规模 |\n"
            "| --- | --- | --- |\n"
            "| 二分查找 | O(log n) | n <= 10^9 |\n"
            "| 快速排序 / 归并排序 | O(n log n) | n <= 10^6 |\n";
        doc_updating_ = true;
        word_editor_->setHtml(render_markdown_for_word_editor(sampleMd, ""));
        updateWordPaperScale();
        doc_updating_ = false;
    }

    void selectDsLesson(const std::string& lessonId) {
        DsLesson les;
        {
            std::lock_guard<std::mutex> lk(ds_mutex_);
            if (!ds_.get_lesson(lessonId, les)) return;
        }
        cur_ds_lesson_ = les;
        ds_les_title_->setText(QString::fromStdString(les.title));
        ds_les_time_->setText(QString("时间 %1").arg(QString::fromStdString(les.time_comp)));
        ds_les_space_->setText(QString("空间 %1").arg(QString::fromStdString(les.space_comp)));
        ds_les_src_->setText(QString::fromStdString(les.source_ref));
        ds_note_inp_->setText(QString::fromStdString(les.note));
        updateDsMasteredBtn();

        if (ds_tut_widget_) ds_tut_widget_->setTutorial(les.tutorial_html, pal_);
        ds_tmpl_editor_->setCodeSafely(QString::fromStdString(les.code_template));

        if (ds_viz_title_lbl_) {
            ds_viz_title_lbl_->setText(QString("🎯 %1").arg(
                QString::fromStdString(les.viz_label.empty() ? les.title : les.viz_label)));
        }
        ds_viz_->setLesson(QString::fromStdString(les.id), QString::fromStdString(les.viz_type));
        if (ds_viz_timer_->isActive()) { ds_viz_timer_->stop(); ds_viz_play_btn_->setText(QStringLiteral("⏯ 自动演示")); }
        syncDsVizLabels();

        cur_ex_id_ = les.exercises.empty() ? "free" : les.exercises[0].id;
        rebuildExTabs();
        loadCurrentDsExercise();
        refreshDsCatalog(false);
    }

private:

    void rebuildExTabs() {
        if (!ds_ex_tabs_layout_) return;
        QWidget* host = ds_ex_tabs_layout_->parentWidget();
        while (QLayoutItem* it = ds_ex_tabs_layout_->takeAt(0)) {
            if (QWidget* w = it->widget()) {
                w->hide();
                w->setParent(nullptr);
                w->deleteLater();
            }
            delete it;
        }
        for (const auto& ex : cur_ds_lesson_.exercises) {
            QString txt = QString("📝 %1%2").arg(QString::fromStdString(ex.title), ex.completed ? QStringLiteral(" ✓") : "");
            auto* b = new QPushButton(txt, host);
            b->setProperty("btn", "seg");
            b->setCheckable(true);
            b->setChecked(ex.id == cur_ex_id_);
            connect(b, &QPushButton::clicked, this, [this, eid = ex.id]() { selectDsExercise(eid); });
            ds_ex_tabs_layout_->addWidget(b);
        }
        auto* fb = new QPushButton(QStringLiteral("⚡ 自由演练"), host);
        fb->setProperty("btn", "seg");
        fb->setCheckable(true);
        fb->setChecked(cur_ex_id_ == "free");
        connect(fb, &QPushButton::clicked, this, [this]() { selectDsExercise("free"); });
        ds_ex_tabs_layout_->addWidget(fb);
    }

    void selectDsExercise(const std::string& eid) {
        cur_ex_id_ = eid;
        rebuildExTabs();
        loadCurrentDsExercise();
    }

    void loadCurrentDsExercise() {
        if (cur_ex_id_ == "free") {
            ds_ide_editor_->setCodeSafely(ds_free_code_);
            ds_ex_prompt_lbl_->setText(QStringLiteral("<b>自由演练模式：</b>可编写任意完整 C / C++17 程序或改造教程模板进行运行测试。"));
            ds_ex_sample_lbl_->setText(QStringLiteral("C/C++17 自由沙盒"));
            ds_ex_mode_pill_->setText(QStringLiteral("模式：自由编程"));
            ds_ex_status_pill_->setText(QStringLiteral("自由模式"));
            ds_submit_btn_->setVisible(false);
        } else {
            for (const auto& ex : cur_ds_lesson_.exercises) {
                if (ex.id == cur_ex_id_) {
                    ds_ide_editor_->setCodeSafely(QString::fromStdString(ex.user_code.empty() ? ex.starter_code : ex.user_code));
                    ds_ex_prompt_lbl_->setText(QString("<b>题目要求：</b>%1").arg(QString::fromStdString(ex.prompt).toHtmlEscaped()));
                    QString sin = QString::fromStdString(ex.sample_input).replace("\n", " ");
                    QString sout = QString::fromStdString(ex.expected_output).replace("\n", " ");
                    ds_ex_sample_lbl_->setText(QString("样例: %1 → %2").arg(sin.left(18), sout.left(18)));
                    ds_ex_mode_pill_->setText(ex.mode == "completion" ? QStringLiteral("模式：核心填空补全") : QStringLiteral("模式：独立实现"));
                    ds_ex_status_pill_->setText(ex.completed || ex.last_status == "PASS" ? QStringLiteral("已自测通过 ✓")
                        : (ex.last_status == "CE" ? QStringLiteral("存在编译错误") : QStringLiteral("待提交评测")));
                    ds_stdin_inp_->setText(sin);
                    ds_submit_btn_->setVisible(true);
                    break;
                }
            }
        }
        ds_console_->setHtml(QStringLiteral("<span style='color:#6B7280;'>点击「✓ 提交自测用例」或按 Ctrl+Enter 本地调用 g++ 编译并运行评测。</span>"));
    }

    void runDsExerciseAsync(bool runTests) {
        std::string code = ds_ide_editor_->toPlainText().toStdString();
        bool isFree = (cur_ex_id_ == "free");
        std::string eid = isFree ? "" : cur_ex_id_;
        std::string lid = cur_ds_lesson_.id;
        std::string customIn = ds_stdin_inp_->text().replace(QStringLiteral("\\n"), QStringLiteral("\n")).toStdString();
        bool doTests = runTests && !isFree;

        ds_run_btn_->setEnabled(false);
        ds_submit_btn_->setEnabled(false);
        ds_console_->setHtml(QStringLiteral("<b style='color:#2563EB;'>⏳ 正在调用本地 g++ -std=c++17 编译执行...</b>"));

        std::thread([this, eid, lid, code, customIn, doTests]() {
            DsRunResult rr;
            {
                std::lock_guard<std::mutex> lk(ds_mutex_);
                rr = ds_.run_and_grade(eid, code, customIn, doTests);
            }
            QMetaObject::invokeMethod(this, [this, rr, eid, lid, doTests]() {
                ds_run_btn_->setEnabled(true);
                ds_submit_btn_->setEnabled(true);
                if (!rr.ok) {
                    ds_console_->setHtml(QString("<pre style='color:#DC2626;'>%1</pre>").arg(QString::fromStdString(rr.stderr_text).toHtmlEscaped()));
                    return;
                }
                if (!rr.compiled) {
                    ds_console_->setHtml(QString("<b style='color:#DC2626;'>✕ 编译失败 (CE · %1ms)</b><pre style='color:#DC2626;'>%2</pre>")
                        .arg(static_cast<int>(rr.compile_ms)).arg(QString::fromStdString(rr.stderr_text).toHtmlEscaped()));
                    return;
                }
                if (doTests && rr.case_results.is_array() && !rr.case_results.arr_val.empty()) {
                    QString html = QString("<b style='color:%1;'>%2 (%3 / %4) · 编译 %5ms</b><br>")
                        .arg(rr.all_passed ? "#059669" : "#D97706",
                             rr.all_passed ? QStringLiteral("✓ 自测全部通过") : QStringLiteral("⚠ 部分用例未通过"))
                        .arg(rr.passed_count).arg(rr.total_count).arg(static_cast<int>(rr.compile_ms));
                    for (const auto& tc : rr.case_results.arr_val) {
                        bool p = tc.get("passed").bool_val;
                        html += QString("<div style='color:%1;'>[%2] %3 — %4</div>")
                            .arg(p ? "#059669" : "#DC2626", p ? "PASS" : "FAIL",
                                 QString::fromStdString(tc.get("label").str_val).toHtmlEscaped(),
                                 p ? QString("输出: %1").arg(QString::fromStdString(tc.get("actual").str_val).toHtmlEscaped())
                                   : QString("期望: %1 | 实际: %2").arg(QString::fromStdString(tc.get("expected").str_val).toHtmlEscaped(),
                                                                      QString::fromStdString(tc.get("actual").str_val).toHtmlEscaped()));
                    }
                    ds_console_->setHtml(html);
                    if (rr.all_passed) {
                        for (auto& ex : cur_ds_lesson_.exercises) {
                            if (ex.id == eid) { ex.completed = true; ex.last_status = "PASS"; }
                        }
                        ds_ex_status_pill_->setText(QStringLiteral("已自测通过 ✓"));
                        rebuildExTabs();
                        refreshDsCatalog(false);
                        showToast(QStringLiteral("🎉 自测全部通过！已持久化至 SQLite3"));
                    }
                } else {
                    QString out = rr.stdout_text.empty() ? QStringLiteral("(程序正常退出，无输出)") : QString::fromStdString(rr.stdout_text).toHtmlEscaped();
                    ds_console_->setHtml(QString("<b style='color:#059669;'>▶ 编译运行成功 (%1ms)</b><pre>%2</pre>")
                        .arg(static_cast<int>(rr.compile_ms)).arg(out));
                }
            }, Qt::QueuedConnection);
        }).detach();
    }

    // ========================================================================
    // PAGE 3: Native Tools & Format Converter Utility
    // ========================================================================
    QWidget* buildToolsPage() {
        auto* page = new QWidget();
        auto* lay = new QVBoxLayout(page);
        lay->setContentsMargins(16, 16, 16, 16);
        lay->setSpacing(12);

        auto* infoCard = makeCard(page);
        auto* icL = new QVBoxLayout(infoCard);
        icL->setContentsMargins(14, 12, 14, 12);
        auto* t1 = new QLabel(QStringLiteral("⚙ 环境状态与本地数据目录"), infoCard);
        t1->setStyleSheet(QStringLiteral("font-weight:700;font-size:14px;"));
        tool_diag_lbl_ = new QLabel(infoCard);
        std::string gpp = DsManager::find_cpp_compiler();
        std::string edge = find_chromium_or_edge();
        tool_diag_lbl_->setText(QString(
            "• UI 引擎：纯原生 Qt 6 (QWidgets + QPainter + QSyntaxHighlighter + QTextDocument，无浏览器内核)\n"
            "• C++ 编译器：%1\n"
            "• 数据目录：%2\n"
            "• PDF 引擎：%3\n"
            "• 词库规模：%4 词 | 数据结构课程库：%5 节 / %6 题")
            .arg(gpp.empty() ? QStringLiteral("未检测到 g++") : QString::fromStdString(gpp),
                 QString::fromStdString(get_app_data_dir()),
                 edge.empty() ? QStringLiteral("内置 C++ PDF 1.4 引擎") : QString::fromStdString(edge))
            .arg(dict_.total_count()).arg(ds_.total_lessons()).arg(ds_.total_exercises()));
        auto* openDataBtn = new QPushButton(QStringLiteral("📂 打开本地数据目录"), infoCard);
        openDataBtn->setProperty("btn", "secondary");
        connect(openDataBtn, &QPushButton::clicked, this, []() { open_in_file_manager(get_app_data_dir()); });
        icL->addWidget(t1);
        icL->addWidget(tool_diag_lbl_);
        icL->addWidget(openDataBtn, 0, Qt::AlignLeft);
        lay->addWidget(infoCard);

        auto* convCard = makeCard(page);
        auto* cvL = new QVBoxLayout(convCard);
        cvL->setContentsMargins(14, 12, 14, 12);
        cvL->setSpacing(8);
        auto* t2 = new QLabel(QStringLiteral("🔄 本地离线文档互转器 (Markdown ↔ Word .docx ↔ A4 PDF)"), convCard);
        t2->setStyleSheet(QStringLiteral("font-weight:700;font-size:14px;"));
        auto* row = new QHBoxLayout();
        tool_in_path_ = new QLineEdit(convCard);
        tool_in_path_->setPlaceholderText(QStringLiteral("选择本地 .md / .docx 文件..."));
        auto* pickF = new QPushButton(QStringLiteral("选择文件"), convCard); pickF->setProperty("btn", "secondary");
        tool_fmt_combo_ = new QComboBox(convCard); tool_fmt_combo_->addItems({"docx", "md", "pdf"});
        auto* goConv = new QPushButton(QStringLiteral("开始转换"), convCard); goConv->setProperty("btn", "primary");
        row->addWidget(tool_in_path_, 1);
        row->addWidget(pickF);
        row->addWidget(tool_fmt_combo_);
        row->addWidget(goConv);
        cvL->addWidget(t2);
        cvL->addLayout(row);
        lay->addWidget(convCard);
        lay->addStretch();

        connect(pickF, &QPushButton::clicked, this, [this]() {
            QString fp = QFileDialog::getOpenFileName(this, QStringLiteral("选择待转换文档"), QString(), QStringLiteral("文档 (*.md *.docx *.txt)"));
            if (!fp.isEmpty()) tool_in_path_->setText(fp);
        });
        connect(goConv, &QPushButton::clicked, this, [this]() {
            QString in = tool_in_path_->text().trimmed();
            if (in.isEmpty()) return;
            std::string fmt = tool_fmt_combo_->currentText().toStdString();
            fs::path p = utf8_to_path(in.toStdString());
            std::string out = path_to_utf8(p.parent_path() / (path_to_utf8(p.stem()) + "_converted." + fmt));
            int rc = run_cli_convert(in.toStdString(), fmt, out);
            if (rc == 0) showToast(QString("已转换输出：%1").arg(QString::fromStdString(out)));
            else QMessageBox::warning(this, QStringLiteral("转换失败"), QStringLiteral("请检查输入文件是否存在及格式正确"));
        });
        return page;
    }
};

} // namespace omni::qt

int main(int argc, char** argv) {
    bool selfTest = false;
    std::string convIn, convTo, convOut, dumpDir;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--self-test") selfTest = true;
        else if (a == "--dump-screenshots" && i + 1 < argc) dumpDir = argv[++i];
        else if (a == "--convert" && i + 1 < argc) convIn = argv[++i];
        else if (a == "--to" && i + 1 < argc) convTo = argv[++i];
        else if ((a == "-o" || a == "--out") && i + 1 < argc) convOut = argv[++i];
    }
    if (!convIn.empty() && !convTo.empty()) {
        return omni::qt::run_cli_convert(convIn, convTo, convOut);
    }

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("OmniWorkbench"));
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    omni::qt::OmniQtWindow win;
    if (selfTest) {
        if (win.dsTotalLessons() < 50 || win.dsTotalExercises() < 55 || win.dictTotalCount() <= 0) {
            std::cerr << "Self-test check failed\n";
            return 2;
        }
        win.switchTab(0, false);
        win.switchTab(1, false);
        win.switchTab(2, false);
        win.switchTab(3, false);
        win.applyTheme("sepia", false);
        win.applyTheme("night", false);
        win.applyTheme("white", false);
        std::cout << "[PASS] Qt6 native UI self-test (" << win.dsTotalLessons() << " lessons, "
                  << win.dsTotalExercises() << " exercises, " << win.dictTotalCount() << " words)\n";
        return 0;
    }

    if (!dumpDir.empty()) {
        QDir().mkpath(QString::fromStdString(dumpDir));
        win.resize(1560, 940);
        win.applyTheme("white", false);
        win.show();

        auto settleAndSave = [&](const QString& fname) {
            for (int k = 0; k < 4; ++k) app.processEvents();
            QString out = QString::fromStdString(dumpDir) + "/" + fname;
            win.grab().save(out);
            std::cout << "[SHOT] " << out.toStdString() << "\n";
        };

        win.switchTab(2, false);
        win.selectDsLesson("5.6");
        settleAndSave(QStringLiteral("tab2_ds_5_6.png"));

        win.selectDsLesson("0.1");
        settleAndSave(QStringLiteral("tab2_ds_0_1.png"));

        win.switchTab(1, false);
        win.prepareSampleDocForScreenshot();
        settleAndSave(QStringLiteral("tab1_word.png"));

        win.switchTab(0, false);
        settleAndSave(QStringLiteral("tab0_english.png"));
        return 0;
    }

    win.show();
    return app.exec();
}
