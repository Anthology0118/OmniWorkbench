#!/usr/bin/env python3
"""
Build / enrich OmniWorkbench modular SQLite3 dictionary (omni_dict.db) using:
  1. kajweb/dict exam books (CET4, CET6, 考研, IELTS, TOEFL, TEM4, TEM8, SAT, GRE)
  2. skywind3000/ECDICT exchange morphology & core lexicon
"""
import csv
import io
import json
import os
import pathlib
import sqlite3
import sys
import urllib.request
import zipfile

KAJWEB_BOOKS = [
    ("CET4",  "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164635506_CET4_2.zip"),
    ("CET6",  "https://raw.githubusercontent.com/kajweb/dict/master/book/1524052554766_CET6_2.zip"),
    ("考研",   "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164654696_KaoYan_2.zip"),
    ("IELTS", "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164657744_IELTS_2.zip"),
    ("TOEFL", "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164640451_TOEFL_2.zip"),
    ("TEM4",  "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164653685_Level4_2.zip"),
    ("TEM8",  "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164663794_Level8_2.zip"),
    ("SAT",   "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164670910_SAT_2.zip"),
    ("GRE",   "https://raw.githubusercontent.com/kajweb/dict/master/book/1521164637271_GRE_2.zip"),
]

ECDICT_CSV_URL = "https://raw.githubusercontent.com/skywind3000/ECDICT/master/ecdict.csv"

EXCHANGE_MAP = {
    "p": "过去式",
    "d": "过去分词",
    "i": "现在分词",
    "3": "第三人称单数",
    "s": "复数",
    "r": "比较级",
    "t": "最高级",
}

TAG_NORM = [
    ("cet4", "CET4"), ("cet6", "CET6"), ("ky", "考研"),
    ("ielts", "IELTS"), ("toefl", "TOEFL"), ("gre", "GRE"),
    ("gk", "高考"), ("zk", "中考"),
]


def fmt_ipa(s: str) -> str:
    s = (s or "").strip()
    if not s:
        return ""
    if (s.startswith("/") and s.endswith("/")) or (s.startswith("[") and s.endswith("]")):
        return s
    return f"/{s}/"


def ensure_schema(conn: sqlite3.Connection) -> None:
    cur = conn.cursor()
    cur.executescript("""
    PRAGMA journal_mode=WAL;
    PRAGMA synchronous=NORMAL;
    CREATE TABLE IF NOT EXISTS words (
      word TEXT PRIMARY KEY,
      ipa TEXT,
      uk_ipa TEXT DEFAULT '',
      pos TEXT,
      usage TEXT,
      example_en TEXT,
      example_zh TEXT,
      note TEXT,
      tags TEXT DEFAULT '',
      exchange TEXT DEFAULT '',
      rem_method TEXT DEFAULT '',
      rel_words TEXT DEFAULT '',
      syno TEXT DEFAULT '',
      marked INTEGER DEFAULT 0,
      favorite INTEGER DEFAULT 0
    );
    CREATE INDEX IF NOT EXISTS idx_words_tags ON words(tags);

    CREATE TABLE IF NOT EXISTS word_phrases (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      word TEXT NOT NULL,
      phrase TEXT NOT NULL,
      translation TEXT DEFAULT '',
      UNIQUE(word, phrase)
    );
    CREATE INDEX IF NOT EXISTS idx_phrases_word ON word_phrases(word);

    CREATE TABLE IF NOT EXISTS word_sentences (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      word TEXT NOT NULL,
      en TEXT NOT NULL,
      zh TEXT DEFAULT '',
      source TEXT DEFAULT '精选例句',
      UNIQUE(word, en)
    );
    CREATE INDEX IF NOT EXISTS idx_sentences_word ON word_sentences(word);

    CREATE TABLE IF NOT EXISTS word_forms (
      inflected TEXT NOT NULL,
      lemma TEXT NOT NULL,
      form_label TEXT DEFAULT '',
      PRIMARY KEY(inflected, lemma)
    );
    CREATE INDEX IF NOT EXISTS idx_forms_inflected ON word_forms(inflected);

    CREATE TABLE IF NOT EXISTS word_notes (
      word TEXT PRIMARY KEY,
      note TEXT DEFAULT '',
      updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );

    CREATE TABLE IF NOT EXISTS reading_articles (
      id INTEGER PRIMARY KEY AUTOINCREMENT,
      title TEXT NOT NULL,
      level TEXT DEFAULT '',
      target_words TEXT DEFAULT '',
      passage_en TEXT DEFAULT '',
      passage_zh TEXT DEFAULT '',
      questions_json TEXT DEFAULT '[]',
      created_at DATETIME DEFAULT CURRENT_TIMESTAMP
    );
    """)
    for col in ("tags", "uk_ipa", "exchange", "rem_method", "rel_words", "syno"):
        try:
            cur.execute(f"ALTER TABLE words ADD COLUMN {col} TEXT DEFAULT '';")
        except sqlite3.OperationalError:
            pass
    conn.commit()


def download_bytes(url: str) -> bytes:
    req = urllib.request.Request(url, headers={"User-Agent": "OmniWorkbench/1.0"})
    with urllib.request.urlopen(req, timeout=60) as resp:
        return resp.read()


def import_kajweb(conn: sqlite3.Connection, cache_dir: pathlib.Path) -> int:
    cur = conn.cursor()
    total = 0
    for tag, url in KAJWEB_BOOKS:
        fname = cache_dir / pathlib.Path(url).name
        if not fname.exists():
            print(f"  [DL] {tag}: {url}")
            fname.write_bytes(download_bytes(url))
        else:
            print(f"  [CACHE] {tag}: {fname.name}")

        with zipfile.ZipFile(fname, "r") as zf:
            jname = [n for n in zf.namelist() if n.lower().endswith(".json")][0]
            raw_lines = zf.read(jname).decode("utf-8", errors="replace").splitlines()

        count = 0
        cur.execute("BEGIN;")
        for line in raw_lines:
            line = line.strip()
            if not line:
                continue
            obj = json.loads(line)
            word = (obj.get("headWord") or obj.get("word") or "").strip().lower()
            if not word:
                continue
            c = obj.get("content", {}).get("word", {}).get("content", {})
            us = fmt_ipa(c.get("usphone") or c.get("phone") or "")
            uk = fmt_ipa(c.get("ukphone") or "")
            ipa = us or uk

            trans_parts = []
            for tr in c.get("trans") or []:
                p = (tr.get("pos") or "").strip()
                cn = (tr.get("tranCn") or tr.get("tranOther") or "").strip()
                if cn:
                    trans_parts.append(f"{p}. {cn}" if p else cn)
            pos = "；".join(trans_parts)

            phrases = []
            for ph in (c.get("phrase", {}).get("phrases") or [])[:8]:
                pc = (ph.get("pContent") or "").strip()
                pcn = (ph.get("pCn") or "").strip()
                if pc:
                    phrases.append((pc, pcn))
            usage = "\n".join(f"• {p}：{t}" if t else f"• {p}" for p, t in phrases)

            sentences = []
            for sn in (c.get("sentence", {}).get("sentences") or [])[:5]:
                en = (sn.get("sContent") or "").strip()
                zh = (sn.get("sCn") or "").strip()
                if en:
                    sentences.append((en, zh, "精选例句"))
            for sn in (c.get("realExamSentence", {}).get("sentences") or [])[:3]:
                en = (sn.get("sContent") or "").strip()
                si = sn.get("sourceInfo") or {}
                src = " ".join(x for x in [si.get("level", ""), si.get("year", ""), si.get("type", "")] if x).strip() or "真题例句"
                if en:
                    sentences.append((en, "", src))
            ex_en = sentences[0][0] if sentences else ""
            ex_zh = sentences[0][1] if sentences else ""

            rem = ""
            if isinstance(c.get("remMethod"), dict):
                rem = (c["remMethod"].get("val") or "").strip()

            rel_parts = []
            for r in (c.get("relWord", {}).get("rels") or [])[:4]:
                rp = (r.get("pos") or "").strip()
                for rw in (r.get("words") or [])[:3]:
                    hw = (rw.get("hwd") or "").strip()
                    tr = (rw.get("tran") or "").strip()
                    if hw:
                        rel_parts.append(f"{rp}. {hw} ({tr})" if rp and tr else (f"{hw} ({tr})" if tr else hw))
            rel_words = "；".join(rel_parts)

            syno_words = []
            for sy in (c.get("syno", {}).get("synos") or [])[:3]:
                for hw in (sy.get("hwds") or [])[:4]:
                    w = (hw.get("w") or "").strip()
                    if w and w not in syno_words:
                        syno_words.append(w)
            syno = ", ".join(syno_words)

            cur.execute("""
                INSERT INTO words (word, ipa, uk_ipa, pos, usage, example_en, example_zh, note, tags, rem_method, rel_words, syno)
                VALUES (?, ?, ?, ?, ?, ?, ?, '', ?, ?, ?, ?)
                ON CONFLICT(word) DO UPDATE SET
                  ipa=CASE WHEN excluded.ipa!='' THEN excluded.ipa ELSE words.ipa END,
                  uk_ipa=CASE WHEN excluded.uk_ipa!='' THEN excluded.uk_ipa ELSE words.uk_ipa END,
                  pos=CASE WHEN excluded.pos!='' THEN excluded.pos ELSE words.pos END,
                  usage=CASE WHEN excluded.usage!='' THEN excluded.usage ELSE words.usage END,
                  example_en=CASE WHEN excluded.example_en!='' THEN excluded.example_en ELSE words.example_en END,
                  example_zh=CASE WHEN excluded.example_zh!='' THEN excluded.example_zh ELSE words.example_zh END,
                  rem_method=CASE WHEN excluded.rem_method!='' THEN excluded.rem_method ELSE words.rem_method END,
                  rel_words=CASE WHEN excluded.rel_words!='' THEN excluded.rel_words ELSE words.rel_words END,
                  syno=CASE WHEN excluded.syno!='' THEN excluded.syno ELSE words.syno END,
                  tags=CASE WHEN words.tags='' THEN excluded.tags
                            WHEN instr(words.tags, excluded.tags)=0 THEN words.tags || ' ' || excluded.tags
                            ELSE words.tags END;
            """, (word, ipa, uk, pos, usage, ex_en, ex_zh, tag, rem, rel_words, syno))

            for pc, pcn in phrases:
                cur.execute("INSERT OR IGNORE INTO word_phrases (word, phrase, translation) VALUES (?, ?, ?);", (word, pc, pcn))
            for en, zh, src in sentences:
                cur.execute("INSERT OR IGNORE INTO word_sentences (word, en, zh, source) VALUES (?, ?, ?, ?);", (word, en, zh, src))
            count += 1

        conn.commit()
        print(f"    -> {tag}: {count} entries")
        total += count
    return total


def import_ecdict_exchange_and_core(conn: sqlite3.Connection, cache_dir: pathlib.Path) -> tuple[int, int]:
    csv_path = cache_dir / "ecdict.csv"
    if not csv_path.exists():
        print(f"  [DL] ECDICT: {ECDICT_CSV_URL}")
        csv_path.write_bytes(download_bytes(ECDICT_CSV_URL))
    else:
        print(f"  [CACHE] ECDICT: {csv_path.name}")

    cur = conn.cursor()
    existing_words = set(r[0] for r in cur.execute("SELECT word FROM words;"))
    updated = 0
    forms_count = 0

    cur.execute("BEGIN;")
    with open(csv_path, "r", encoding="utf-8", errors="replace", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            word = (row.get("word") or "").strip().lower()
            if not word or " " in word:
                continue
            exch = (row.get("exchange") or "").strip()
            tag_raw = (row.get("tag") or "").strip().lower()
            bnc = int(row.get("bnc") or 0) if (row.get("bnc") or "").isdigit() else 0
            frq = int(row.get("frq") or 0) if (row.get("frq") or "").isdigit() else 0

            # Keep if already in kajweb exam books OR has exam tag OR high-frequency BNC/COCA core word (<= 30000 rank)
            is_core = (word in existing_words) or bool(tag_raw) or (0 < bnc <= 25000) or (0 < frq <= 25000)
            if not is_core:
                continue

            ipa = fmt_ipa(row.get("phonetic") or "")
            trans = (row.get("translation") or "").replace("\\n", "；").strip()
            norm_tags = " ".join(norm for k, norm in TAG_NORM if k in tag_raw)

            if word in existing_words:
                cur.execute("""
                    UPDATE words SET
                      exchange = CASE WHEN ?!='' THEN ? ELSE exchange END,
                      ipa = CASE WHEN ipa='' AND ?!='' THEN ? ELSE ipa END,
                      pos = CASE WHEN pos='' AND ?!='' THEN ? ELSE pos END,
                      tags = CASE WHEN tags='' AND ?!='' THEN ?
                                  WHEN ?!='' AND instr(tags, ?)=0 THEN tags || ' ' || ?
                                  ELSE tags END
                    WHERE word = ?;
                """, (exch, exch, ipa, ipa, trans, trans, norm_tags, norm_tags, norm_tags, norm_tags, norm_tags, word))
            else:
                cur.execute("""
                    INSERT OR IGNORE INTO words (word, ipa, pos, tags, exchange)
                    VALUES (?, ?, ?, ?, ?);
                """, (word, ipa, trans, norm_tags, exch))
                existing_words.add(word)
            updated += 1

            if exch:
                for part in exch.split("/"):
                    if ":" not in part:
                        continue
                    code, val = part.split(":", 1)
                    code = code.strip()
                    val = val.strip().lower()
                    if code in EXCHANGE_MAP and val and val != word:
                        cur.execute(
                            "INSERT OR IGNORE INTO word_forms (inflected, lemma, form_label) VALUES (?, ?, ?);",
                            (val, word, EXCHANGE_MAP[code]),
                        )
                        forms_count += 1

    conn.commit()
    return updated, forms_count


def main():
    default_db = pathlib.Path("/mnt/c/Users/34970/AppData/Local/OmniWorkbench/omni_dict.db")
    db_path = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else default_db
    db_path.parent.mkdir(parents=True, exist_ok=True)
    cache_dir = pathlib.Path("/tmp/omni_dict_cache")
    cache_dir.mkdir(parents=True, exist_ok=True)

    print("Target SQLite DB:", db_path)
    conn = sqlite3.connect(str(db_path))
    ensure_schema(conn)

    print("[1/2] Importing kajweb/dict exam books...")
    import_kajweb(conn, cache_dir)

    print("[2/2] Importing ECDICT exchange morphology & core lexicon...")
    updated, forms = import_ecdict_exchange_and_core(conn, cache_dir)

    words_n = conn.execute("SELECT COUNT(*) FROM words;").fetchone()[0]
    phrases_n = conn.execute("SELECT COUNT(*) FROM word_phrases;").fetchone()[0]
    sents_n = conn.execute("SELECT COUNT(*) FROM word_sentences;").fetchone()[0]
    forms_n = conn.execute("SELECT COUNT(*) FROM word_forms;").fetchone()[0]
    conn.close()

    size_mb = db_path.stat().st_size / (1024 * 1024)
    print(f"Done! DB size: {size_mb:.2f} MB | words={words_n}, phrases={phrases_n}, sentences={sents_n}, word_forms={forms_n} (ecdict_touched={updated}, forms_added={forms})")


if __name__ == "__main__":
    main()
