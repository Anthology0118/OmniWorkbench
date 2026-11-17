#!/usr/bin/env python3
"""
Populate modular reading_articles + FTS5 index in omni_dict.db with a comprehensive
graduate & exam reading comprehension + cloze + long-article corpus:
  - LSAT (LSAT-RC & LSAT-LR graduate law-school reading & reasoning, AGIEval)
  - ReClor (GMAT & LSAT graduate logical reading comprehension, ~5,138 items)
  - QuALITY (long 3,000-5,000 word college/graduate reading comprehension)
  - SAT (College Board SAT academic reading + official explanations, AGIEval)
  - LogiQA (English logical reading comprehension, AGIEval)
  - Gaokao (Gaokao English reading, AGIEval)
  - RACE (High-emphasis large batch + Middle, ~3,500+ passages)
  - CLOTH (All valid High + Middle cloze tests from data.tar.gz, ~6,500+ passages)
  - BBC News, TED EN-ZH Bilingual, SimpleWiki, TinyStories, Project Gutenberg
"""

import concurrent.futures
import io
import json
import os
import re
import shutil
import sqlite3
import tarfile
import time
import urllib.parse
import urllib.request
import zipfile

DB_TARGET = "/mnt/c/Users/34970/AppData/Local/OmniWorkbench/omni_dict.db"
CACHE_DIR = "/tmp/omni_reading_cache"
WORK_DB = "/tmp/omni_dict_reading_work.db"

os.makedirs(CACHE_DIR, exist_ok=True)
LETTERS = ["A", "B", "C", "D", "E"]


def fetch_url(url: str, cache_name: str) -> bytes:
    path = os.path.join(CACHE_DIR, cache_name)
    if os.path.exists(path) and os.path.getsize(path) > 0:
        with open(path, "rb") as f:
            return f.read()
    print(f"  [DL] {url}", flush=True)
    req = urllib.request.Request(url, headers={"User-Agent": "OmniWorkbench/1.0"})
    with urllib.request.urlopen(req, timeout=120) as resp:
        data = resp.read()
    with open(path, "wb") as f:
        f.write(data)
    return data


def _fetch_hf_page(args):
    dataset, config, split, offset, length = args
    url = (
        f"https://datasets-server.huggingface.co/rows?"
        f"dataset={urllib.parse.quote(dataset)}&config={urllib.parse.quote(config)}"
        f"&split={urllib.parse.quote(split)}&offset={offset}&length={length}"
    )
    for attempt in range(4):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": "OmniWorkbench/1.0"})
            with urllib.request.urlopen(req, timeout=45) as resp:
                payload = json.loads(resp.read().decode("utf-8"))
            return [item.get("row", {}) for item in payload.get("rows", [])]
        except Exception:
            if attempt == 3:
                return []
            time.sleep(1.0 + attempt)
    return []


def fetch_hf_rows(dataset: str, config: str, split: str, total_rows: int, page_size: int = 100):
    safe_tag = f"{dataset}_{config}_{split}_{total_rows}".replace("/", "_").replace(".", "_")
    cache_path = os.path.join(CACHE_DIR, f"hf_{safe_tag}.json")
    if os.path.exists(cache_path) and os.path.getsize(path_or := cache_path) > 10:
        with open(path_or, "r", encoding="utf-8") as f:
            return json.load(f)

    print(f"  [HF] Fetching {total_rows} rows from {dataset} ({config}/{split})...", flush=True)
    tasks = [
        (dataset, config, split, off, min(page_size, total_rows - off))
        for off in range(0, total_rows, page_size)
    ]
    rows = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=4) as pool:
        for batch in pool.map(_fetch_hf_page, tasks):
            rows.extend(batch)
    if rows:
        with open(cache_path, "w", encoding="utf-8") as f:
            json.dump(rows, f, ensure_ascii=False)
    return rows


def clean_title_from_text(text: str, prefix: str = "") -> str:
    line = re.split(r"[.\n!?]", text.strip(), maxsplit=1)[0].strip()
    words = line.split()
    snippet = " ".join(words[:10])
    if len(words) > 10:
        snippet += "..."
    if not snippet:
        snippet = "English Reading Passage"
    return f"{prefix}{snippet}" if prefix else snippet


def extract_target_words(text: str, vocab_set: set, limit: int = 12) -> str:
    seen = []
    seen_set = set()
    for tok in re.findall(r"\b[a-z]{4,14}\b", text.lower()):
        if tok in vocab_set and tok not in seen_set:
            seen_set.add(tok)
            seen.append(tok)
            if len(seen) >= limit:
                break
    return " ".join(seen)


def _strip_opt_prefix(s: str) -> str:
    return re.sub(r"^\([A-E]\)\s*", "", str(s or "").strip())


def load_agieval_grouped(file_name: str, source: str, level_label: str, vocab_set: set):
    url = f"https://raw.githubusercontent.com/ruixiangcui/AGIEval/main/data/v1/{file_name}"
    raw = fetch_url(url, f"agieval_{file_name}").decode("utf-8", errors="ignore")
    grouped = {}
    order = []
    for idx, line in enumerate(raw.splitlines()):
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except Exception:
            continue
        passage = (obj.get("passage") or "").strip()
        q = (obj.get("question") or "").strip()
        opts = [_strip_opt_prefix(o) for o in (obj.get("options") or [])]
        lbl = str(obj.get("label") or "").strip().upper()
        if isinstance(obj.get("label"), int) and 0 <= obj["label"] < len(LETTERS):
            lbl = LETTERS[obj["label"]]
        if not passage or not q or not opts or lbl not in LETTERS[:len(opts)]:
            continue
        exp = ""
        other = obj.get("other")
        if isinstance(other, dict):
            exp = str(other.get("solution") or other.get("explanation") or "").strip()

        key = passage
        if key not in grouped:
            grouped[key] = []
            order.append(key)
        grouped[key].append({
            "question": q,
            "options": opts[:5],
            "answer": lbl,
            **({"explanation": exp} if exp else {}),
        })

    out = []
    for passage in order:
        qs = grouped[passage]
        title = clean_title_from_text(passage)
        out.append((
            title,
            source,
            "reading",
            level_label,
            extract_target_words(passage, vocab_set),
            passage,
            "",
            json.dumps(qs, ensure_ascii=False),
        ))
    print(f"    -> {source} ({file_name}): {len(out)} passages, {sum(len(grouped[p]) for p in order)} questions", flush=True)
    return out


def load_reclor(vocab_set: set):
    out = []
    q_total = 0
    for split, lvl in [("val", "GMAT/LSAT 逻辑阅读 (Val)"), ("train", "GMAT/LSAT 逻辑阅读 (Train)")]:
        url = f"https://huggingface.co/datasets/tasksource/reclor/resolve/main/{split}.json"
        raw = fetch_url(url, f"reclor_{split}.json").decode("utf-8", errors="ignore")
        try:
            arr = json.loads(raw)
        except Exception:
            continue
        # Group items sharing the same context passage if any
        grouped = {}
        order = []
        for item in arr:
            ctx = (item.get("context") or "").strip()
            q = (item.get("question") or "").strip()
            opts = [str(x).strip() for x in (item.get("answers") or [])]
            lbl_idx = item.get("label")
            if not ctx or not q or len(opts) < 2 or not isinstance(lbl_idx, int) or not (0 <= lbl_idx < len(opts)):
                continue
            if ctx not in grouped:
                grouped[ctx] = []
                order.append(ctx)
            grouped[ctx].append({
                "question": q,
                "options": opts[:5],
                "answer": LETTERS[lbl_idx],
            })
        for ctx in order:
            qs = grouped[ctx]
            q_total += len(qs)
            out.append((
                clean_title_from_text(ctx),
                "ReClor",
                "reading",
                lvl,
                extract_target_words(ctx, vocab_set),
                ctx,
                "",
                json.dumps(qs, ensure_ascii=False),
            ))
    print(f"    -> ReClor: {len(out)} passages, {q_total} questions", flush=True)
    return out


def load_quality(vocab_set: set):
    out = []
    seen_ids = set()
    q_total = 0
    for split, lvl in [("dev", "大学/研考长难文 (QuALITY Dev)"), ("train", "大学/研考长难文 (QuALITY Train)")]:
        url = f"https://raw.githubusercontent.com/nyu-mll/quality/main/data/v1.0.1/QuALITY.v1.0.1.htmlstripped.{split}"
        raw = fetch_url(url, f"quality_{split}.jsonl").decode("utf-8", errors="ignore")
        for line in raw.splitlines():
            line = line.strip()
            if not line:
                continue
            try:
                obj = json.loads(line)
            except Exception:
                continue
            aid = obj.get("article_id") or obj.get("set_unique_id")
            if aid in seen_ids:
                continue
            seen_ids.add(aid)
            art = (obj.get("article") or "").strip()
            title = (obj.get("title") or "").strip() or clean_title_from_text(art)
            raw_qs = obj.get("questions") or []
            qs = []
            for rq in raw_qs:
                q_txt = (rq.get("question") or "").strip()
                opts = [str(x).strip() for x in (rq.get("options") or [])]
                gl = rq.get("gold_label")
                if not q_txt or len(opts) < 2 or not isinstance(gl, int) or not (1 <= gl <= len(opts)):
                    continue
                qs.append({
                    "question": q_txt,
                    "options": opts[:4],
                    "answer": LETTERS[gl - 1],
                })
            if not art or not qs:
                continue
            # Cap ultra-huge single novel excerpts at ~14,000 chars so UI stays snappy
            art_clean = re.sub(r"\n{3,}", "\n\n", art)[:14000]
            q_total += len(qs)
            out.append((
                title,
                "QuALITY",
                "reading",
                lvl,
                extract_target_words(art_clean, vocab_set),
                art_clean,
                "",
                json.dumps(qs, ensure_ascii=False),
            ))
    print(f"    -> QuALITY: {len(out)} long articles, {q_total} questions", flush=True)
    return out


def load_race_articles(vocab_set: set):
    out = []
    # High-level passages prioritized (test + validation + 6,000 train rows) + middle test
    specs = [
        ("high", "test", "高中/高阶阅读 (RACE-High)", 3500),
        ("high", "validation", "高中/高阶阅读 (RACE-High)", 3500),
        ("high", "train", "高中/高阶阅读 (RACE-High)", 6000),
        ("middle", "test", "初中阅读 (RACE-Middle)", 2200),
    ]
    grouped = {}
    order = []
    for cfg, split, level_label, want_rows in specs:
        rows = fetch_hf_rows("ehovy/race", cfg, split, want_rows, 100)
        for r in rows:
            ex_id = f"{cfg}:{split}:{r.get('example_id', '')}"
            art = (r.get("article") or "").strip()
            q = (r.get("question") or "").strip()
            opts = r.get("options") or []
            ans = (r.get("answer") or "").strip().upper()
            if not art or not q or len(opts) < 2 or ans not in ("A", "B", "C", "D"):
                continue
            if ex_id not in grouped:
                grouped[ex_id] = {"article": art, "level": level_label, "questions": []}
                order.append(ex_id)
            grouped[ex_id]["questions"].append({
                "question": q,
                "options": [str(x).strip() for x in opts[:4]],
                "answer": ans,
            })

    q_total = 0
    for ex_id in order:
        data = grouped[ex_id]
        if not data["questions"]:
            continue
        art = data["article"]
        q_total += len(data["questions"])
        out.append((
            clean_title_from_text(art),
            "RACE",
            "reading",
            data["level"],
            extract_target_words(art, vocab_set),
            art,
            "",
            json.dumps(data["questions"], ensure_ascii=False),
        ))
    print(f"    -> RACE total: {len(out)} passages, {q_total} questions", flush=True)
    return out


def load_cloth_articles(vocab_set: set):
    url = "https://github.com/zhaowei8188127/Large-scale-Cloze-Test-Dataset-Designed-by-Teachers/raw/master/data.tar.gz"
    raw = fetch_url(url, "cloth_data.tar.gz")
    high_rows = []
    mid_rows = []
    with tarfile.open(fileobj=io.BytesIO(raw), mode="r:gz") as tf:
        for member in tf.getmembers():
            if not member.isfile() or not member.name.endswith(".json"):
                continue
            is_mid = "/middle/" in member.name
            is_high = "/high/" in member.name
            if not (is_mid or is_high):
                continue
            f = tf.extractfile(member)
            if not f:
                continue
            try:
                obj = json.loads(f.read().decode("utf-8", errors="ignore"))
            except Exception:
                continue
            art = (obj.get("article") or "").strip()
            opts_list = obj.get("options") or []
            ans_list = obj.get("answers") or []
            if not art or not opts_list or len(opts_list) != len(ans_list):
                continue
            blanks_in_text = len(re.findall(r"(?<!\w)_(?!\w)", art))
            if blanks_in_text != len(ans_list) or len(ans_list) < 3 or len(ans_list) > 25:
                continue
            qs = []
            valid = True
            for opts, ans in zip(opts_list, ans_list):
                ans_u = str(ans).strip().upper()
                if ans_u not in ("A", "B", "C", "D") or len(opts) < 4:
                    valid = False
                    break
                qs.append({
                    "options": [str(x).strip() for x in opts[:4]],
                    "answer": ans_u,
                })
            if not valid:
                continue
            lvl = "高中完形 (CLOTH-High)" if is_high else "初中完形 (CLOTH-Middle)"
            row = (
                clean_title_from_text(art.replace("_", "____"), "[完形] "),
                "CLOTH",
                "cloze",
                lvl,
                extract_target_words(art, vocab_set),
                art,
                "",
                json.dumps(qs, ensure_ascii=False),
            )
            if is_high:
                high_rows.append(row)
            else:
                mid_rows.append(row)
    print(f"    -> CLOTH ALL: high={len(high_rows)}, middle={len(mid_rows)}", flush=True)
    return high_rows + mid_rows


def load_bbc_articles(vocab_set: set):
    rows = fetch_hf_rows("SetFit/bbc-news", "default", "train", 500, 100)
    out = []
    for r in rows:
        txt = (r.get("text") or "").strip()
        cat = (r.get("label_text") or "news").strip()
        if len(txt) < 200:
            continue
        sents = re.split(r"(?<=[.!?])\s+", txt)
        paras = [" ".join(sents[i:i + 4]) for i in range(0, len(sents), 4)]
        formatted = "\n\n".join(paras)
        out.append((
            clean_title_from_text(txt),
            "BBC",
            "reading",
            f"BBC 外刊 · {cat}",
            extract_target_words(formatted, vocab_set),
            formatted,
            "",
            "[]",
        ))
    print(f"    -> BBC News: {len(out)} articles", flush=True)
    return out


def load_ted_bilingual(vocab_set: set):
    url = "https://object.pouta.csc.fi/OPUS-TED2020/v1/moses/en-zh.txt.zip"
    raw = fetch_url(url, "ted2020_en_zh.zip")
    out = []
    with zipfile.ZipFile(io.BytesIO(raw)) as zf:
        en_name = next((n for n in zf.namelist() if n.endswith(".en")), None)
        zh_name = next((n for n in zf.namelist() if n.endswith(".zh")), None)
        if not en_name or not zh_name:
            return out
        with zf.open(en_name) as fe, zf.open(zh_name) as fz:
            pairs = []
            for le, lz in zip(fe, fz):
                if len(pairs) >= 4800:
                    break
                en_s = le.decode("utf-8", errors="ignore").strip()
                zh_s = lz.decode("utf-8", errors="ignore").strip()
                if len(en_s) < 35 or len(zh_s) < 8:
                    continue
                if en_s.startswith("(") or "♪" in en_s:
                    continue
                pairs.append((en_s, zh_s))

            for i in range(0, len(pairs) - 5, 6):
                chunk = pairs[i:i + 6]
                en_body = "\n\n".join(p[0] for p in chunk)
                zh_body = "\n\n".join(p[1] for p in chunk)
                out.append((
                    clean_title_from_text(chunk[0][0], "[TED] "),
                    "TED",
                    "bilingual",
                    "TED 双语精读",
                    extract_target_words(en_body, vocab_set),
                    en_body,
                    zh_body,
                    "[]",
                ))
    print(f"    -> TED Bilingual: {len(out)} passages", flush=True)
    return out


def load_simple_wiki(vocab_set: set):
    rows = fetch_hf_rows("wikimedia/wikipedia", "20231101.simple", "train", 400, 100)
    out = []
    for r in rows:
        title = (r.get("title") or "").strip()
        txt = (r.get("text") or "").strip()
        if not title or len(txt) < 180:
            continue
        txt = txt[:3500]
        out.append((
            title,
            "SimpleWiki",
            "reading",
            "简明英语维基",
            extract_target_words(txt, vocab_set),
            txt,
            "",
            "[]",
        ))
    print(f"    -> SimpleWiki: {len(out)} articles", flush=True)
    return out


def load_tinystories(vocab_set: set):
    rows = fetch_hf_rows("roneneldan/TinyStories", "default", "train", 400, 100)
    out = []
    for r in rows:
        txt = (r.get("text") or "").strip()
        if len(txt) < 120:
            continue
        out.append((
            clean_title_from_text(txt, "[Story] "),
            "TinyStories",
            "literature",
            "短篇每日泛读",
            extract_target_words(txt, vocab_set),
            txt,
            "",
            "[]",
        ))
    print(f"    -> TinyStories: {len(out)} stories", flush=True)
    return out


def load_gutenberg_classics(vocab_set: set):
    books = [
        ("1342", "Pride and Prejudice", "名著 · 傲慢与偏见"),
        ("1661", "Adventures of Sherlock Holmes", "名著 · 福尔摩斯探案集"),
        ("11", "Alice's Adventures in Wonderland", "名著 · 爱丽丝梦游仙境"),
        ("64317", "The Great Gatsby", "名著 · 了不起的盖茨比"),
    ]
    out = []
    for book_id, book_name, lvl in books:
        url = f"https://www.gutenberg.org/cache/epub/{book_id}/pg{book_id}.txt"
        try:
            raw = fetch_url(url, f"pg{book_id}.txt").decode("utf-8", errors="ignore")
        except Exception as exc:
            print(f"    [WARN] Gutenberg {book_id} failed: {exc}", flush=True)
            continue
        start_m = re.search(r"\*\*\* START OF THE PROJECT GUTENBERG EBOOK .*? \*\*\*", raw)
        end_m = re.search(r"\*\*\* END OF THE PROJECT GUTENBERG EBOOK .*? \*\*\*", raw)
        body = raw[start_m.end():end_m.start()] if (start_m and end_m) else raw
        body = body.replace("\r\n", "\n").strip()

        paras = [re.sub(r"\s+", " ", p.strip()) for p in body.split("\n\n") if len(p.strip()) > 30]
        cur = []
        cur_len = 0
        part = 1
        for p in paras:
            cur.append(p)
            cur_len += len(p)
            if cur_len >= 1800:
                chunk_text = "\n\n".join(cur)
                out.append((
                    f"{book_name} (Part {part})",
                    "Gutenberg",
                    "literature",
                    lvl,
                    extract_target_words(chunk_text, vocab_set),
                    chunk_text,
                    "",
                    "[]",
                ))
                part += 1
                cur = []
                cur_len = 0
                if part > 35:
                    break
        if cur and part <= 35:
            chunk_text = "\n\n".join(cur)
            out.append((
                f"{book_name} (Part {part})",
                "Gutenberg",
                "literature",
                lvl,
                extract_target_words(chunk_text, vocab_set),
                chunk_text,
                "",
                "[]",
            ))
    print(f"    -> Gutenberg Classics: {len(out)} sections", flush=True)
    return out


def main():
    if not os.path.exists(DB_TARGET):
        raise FileNotFoundError(f"Missing dictionary DB at {DB_TARGET}")

    print(f"Copying {DB_TARGET} -> {WORK_DB} for fast Linux FTS5 build...", flush=True)
    shutil.copy2(DB_TARGET, WORK_DB)

    conn = sqlite3.connect(WORK_DB)
    cur = conn.cursor()

    vocab_set = {row[0] for row in cur.execute("SELECT word FROM words;").fetchall() if row[0]}
    print(f"Loaded {len(vocab_set)} vocabulary headwords for target highlighting.", flush=True)

    cur.execute(
        """
        CREATE TABLE IF NOT EXISTS reading_articles (
          id INTEGER PRIMARY KEY AUTOINCREMENT,
          title TEXT NOT NULL,
          source TEXT DEFAULT '',
          kind TEXT DEFAULT 'reading',
          level TEXT DEFAULT '',
          target_words TEXT DEFAULT '',
          passage_en TEXT DEFAULT '',
          passage_zh TEXT DEFAULT '',
          questions_json TEXT DEFAULT '[]',
          created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        );
        """
    )
    for col_def in [
        "ALTER TABLE reading_articles ADD COLUMN source TEXT DEFAULT '';",
        "ALTER TABLE reading_articles ADD COLUMN kind TEXT DEFAULT 'reading';",
    ]:
        try:
            cur.execute(col_def)
        except sqlite3.OperationalError:
            pass

    cur.execute("CREATE INDEX IF NOT EXISTS idx_reading_source ON reading_articles(source);")
    cur.execute("CREATE INDEX IF NOT EXISTS idx_reading_kind ON reading_articles(kind);")
    cur.execute(
        """
        CREATE VIRTUAL TABLE IF NOT EXISTS reading_fts USING fts5(
          title, passage_en, content='reading_articles', content_rowid='id'
        );
        """
    )

    cur.execute("DELETE FROM reading_articles;")
    conn.commit()

    all_rows = []
    print("[1/11] LSAT-RC & LSAT-LR (Graduate Law School Reading & Reasoning)...", flush=True)
    all_rows.extend(load_agieval_grouped("lsat-rc.jsonl", "LSAT", "研考/法学院阅读 (LSAT-RC)", vocab_set))
    all_rows.extend(load_agieval_grouped("lsat-lr.jsonl", "LSAT", "研考/逻辑阅读 (LSAT-LR)", vocab_set))

    print("[2/11] ReClor (GMAT & LSAT Graduate Logical Reading)...", flush=True)
    all_rows.extend(load_reclor(vocab_set))

    print("[3/11] QuALITY (Long College/Graduate Reading Comprehension)...", flush=True)
    all_rows.extend(load_quality(vocab_set))

    print("[4/11] SAT-EN (College Board Academic Reading + Explanations)...", flush=True)
    all_rows.extend(load_agieval_grouped("sat-en.jsonl", "SAT", "SAT 学术阅读 (含官方解析)", vocab_set))

    print("[5/11] LogiQA-EN & Gaokao-English...", flush=True)
    all_rows.extend(load_agieval_grouped("logiqa-en.jsonl", "LogiQA", "LogiQA 英文逻辑阅读", vocab_set))
    all_rows.extend(load_agieval_grouped("gaokao-english.jsonl", "Gaokao", "高考英语真题", vocab_set))

    print("[6/11] RACE (High-emphasis large batch + Middle)...", flush=True)
    all_rows.extend(load_race_articles(vocab_set))

    print("[7/11] CLOTH (All valid High + Middle cloze tests)...", flush=True)
    all_rows.extend(load_cloth_articles(vocab_set))

    print("[8/11] BBC News...", flush=True)
    all_rows.extend(load_bbc_articles(vocab_set))

    print("[9/11] TED Talks EN-ZH Bilingual...", flush=True)
    all_rows.extend(load_ted_bilingual(vocab_set))

    print("[10/11] SimpleWiki & TinyStories...", flush=True)
    all_rows.extend(load_simple_wiki(vocab_set))
    all_rows.extend(load_tinystories(vocab_set))

    print("[11/11] Project Gutenberg Classics...", flush=True)
    all_rows.extend(load_gutenberg_classics(vocab_set))

    cur.executemany(
        """
        INSERT INTO reading_articles
          (title, source, kind, level, target_words, passage_en, passage_zh, questions_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?);
        """,
        all_rows,
    )
    print("Rebuilding FTS5 index...", flush=True)
    cur.execute("INSERT INTO reading_fts(reading_fts) VALUES('rebuild');")
    conn.commit()
    cur.execute("PRAGMA wal_checkpoint(TRUNCATE);")
    cur.execute("PRAGMA journal_mode=DELETE;")
    conn.commit()

    by_src = cur.execute("SELECT source, COUNT(*) FROM reading_articles GROUP BY source ORDER BY COUNT(*) DESC;").fetchall()
    total_rd = cur.execute("SELECT COUNT(*) FROM reading_articles;").fetchone()[0]
    fts_test = cur.execute("SELECT COUNT(*) FROM reading_fts WHERE reading_fts MATCH '\"apply\"';").fetchone()[0]
    conn.close()

    shutil.copy2(WORK_DB, DB_TARGET)
    for ext in ("-wal", "-shm"):
        p = DB_TARGET + ext
        if os.path.exists(p):
            try:
                os.remove(p)
            except OSError:
                pass
    if os.path.exists(WORK_DB):
        os.remove(WORK_DB)

    size_mb = os.path.getsize(DB_TARGET) / (1024 * 1024)
    print(f"Done! Total reading_articles={total_rd} | DB size={size_mb:.2f} MB | FTS5 'apply' hits={fts_test}", flush=True)
    print("Breakdown by source:", by_src, flush=True)


if __name__ == "__main__":
    main()
