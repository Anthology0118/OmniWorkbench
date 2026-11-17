#pragma once

#include "json.hpp"

#include <string>
#include <vector>

namespace omni {

struct DictPhrase {
    std::string phrase;
    std::string translation;
};

struct DictSentence {
    std::string en;
    std::string zh;
    std::string source;
};

struct DictForm {
    std::string form_label; // e.g. "过去式", "过去分词", "复数"
    std::string word;
};

struct DictEntry {
    std::string word;
    std::string ipa;        // primary IPA (e.g. usphone or ECDICT phonetic)
    std::string uk_ipa;     // UK IPA if available
    std::string pos;
    std::vector<std::string> usage; // formatted summary phrases for compatibility
    std::vector<DictPhrase> phrases;
    std::vector<DictSentence> sentences;
    std::vector<DictForm> forms;
    std::string exchange;   // raw ECDICT exchange string
    std::string rem_method; // mnemonic / memory tip
    std::string rel_words;  // cognate words summary
    std::string syno;       // synonyms summary
    std::string example_en;
    std::string example_zh;
    std::string note;
    std::string tags;       // e.g. "CET4 CET6 考研 IELTS"
    std::string matched_from; // if found via lemma reverse lookup (e.g. "went (过去式)")
    bool marked = false;
    bool favorite = false;
};

struct ReadingArticle {
    int id = 0;
    std::string title;
    std::string source;        // RACE, CLOTH, BBC, TED, SimpleWiki, TinyStories, Gutenberg
    std::string kind;          // reading, cloze, bilingual, literature
    std::string level;
    std::string target_words;
    std::string passage_en;
    std::string passage_zh;
    std::string questions_json;
    int question_count = 0;
};

struct ReadingWordMark {
    int article_id = 0;
    std::string word;
    std::string surface;
    std::string color;         // yellow | red | blue | green | purple (empty = remove)
    std::string meaning;
    std::string context;
};

/**
 * Modular multi-table SQLite3 offline dictionary engine:
 *  - words            : core lemma, US/UK IPA, POS, exam tags, ECDICT exchange, mnemonics
 *  - word_phrases     : collocations & phrases
 *  - word_sentences   : bilingual & real-exam sentences
 *  - word_forms       : inflected -> lemma reverse index (from ECDICT exchange)
 *  - word_notes       : user personal notes (decoupled from dictionary imports)
 *  - reading_articles : reading comprehension (RACE), cloze (CLOTH), articles & bilingual passages
 */
class DictManager {
public:
    explicit DictManager(std::string db_path = "");
    ~DictManager();

    DictManager(const DictManager&) = delete;
    DictManager& operator=(const DictManager&) = delete;

    bool init();
    std::vector<DictEntry> search(const std::string& query,
                                  int limit = 200,
                                  const std::string& tag_filter = "",
                                  bool marked_only = false,
                                  bool include_details = true,
                                  int offset = 0) const;
    int count_matches(const std::string& query,
                      const std::string& tag_filter = "",
                      bool marked_only = false) const;
    std::vector<DictEntry> similar_words(const std::string& anchor_word,
                                         const std::string& tag_filter = "",
                                         int limit = 60,
                                         int offset = 0,
                                         int* out_total = nullptr) const;
    std::vector<DictEntry> search_by_phonetic(const std::string& combo,
                                            const std::string& ipa_fragment,
                                            int limit = 60,
                                            int offset = 0) const;
    int count_by_phonetic(const std::string& combo,
                          const std::string& ipa_fragment) const;
    bool get_word(const std::string& word, DictEntry& out) const;
    bool set_marked(const std::string& word, bool marked);
    bool set_favorite(const std::string& word, bool favorite);
    bool save_note(const std::string& word, const std::string& note);
    bool upsert_word(const DictEntry& entry);
    int total_count() const;

    /**
     * Import ZIP/JSON/NDJSON (kajweb/dict, KyleBing) or CSV (skywind3000/ECDICT).
     */
    bool import_dictionary_file(const std::string& file_path,
                                const std::string& book_tag_hint,
                                int& out_imported,
                                std::string* err = nullptr);

    JsonValue entry_to_json(const DictEntry& e) const;

    // 长文应用与真题练习模块 (reading_articles + reading_fts)
    std::vector<ReadingArticle> search_readings(const std::string& query = "",
                                                const std::string& source = "",
                                                const std::string& kind = "",
                                                int limit = 24,
                                                int offset = 0) const;
    void reading_stats(const std::string& query,
                       const std::string& source,
                       const std::string& kind,
                       int& out_articles,
                       int& out_questions) const;
    bool get_reading(int id, ReadingArticle& out) const;
    int reading_count() const;
    int total_reading_questions() const;
    JsonValue reading_to_json(const ReadingArticle& r, bool include_body = true) const;
    std::vector<ReadingWordMark> get_reading_marks(int article_id) const;
    bool set_reading_mark(const ReadingWordMark& mark);
    JsonValue marks_to_json(const std::vector<ReadingWordMark>& marks) const;

private:
    std::string db_path_;
    void* db_handle_ = nullptr; // sqlite3*
    bool sqlite_ready_ = false;
    bool reading_fts_ready_ = false;
    std::vector<DictEntry> fallback_store_;
    std::vector<ReadingArticle> fallback_readings_;
    std::vector<ReadingWordMark> fallback_marks_;

    void seed_initial_words();
    bool exec_sql(const std::string& sql);
    bool upsert_word_internal(const DictEntry& entry);
    void populate_child_tables(DictEntry& e) const;
};

std::vector<DictForm> parse_ecdict_exchange(const std::string& exchange);

} // namespace omni
