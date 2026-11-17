#!/usr/bin/env python3
"""
Append & index authentic CET-4, CET-6, and Kaoyan (1998-2025 English I & II)
exam passages & questions into omni_dict.db:
  - Kaoyan: LIziak112/structured-kaoyan-english (44 structured JSON exam papers)
  - CET6 & CET4: extracted from bundled cet6_seed.db (122 CET-4/6 papers: reading1, reading2, matching, cloze)
"""

import concurrent.futures
import json
import os
import re
import shutil
import sqlite3
import urllib.request

DB_TARGET = "/mnt/c/Users/34970/AppData/Local/OmniWorkbench/omni_dict.db"
CACHE_DIR = "/tmp/omni_reading_cache"
CET_SEED_DB = os.path.join(CACHE_DIR, "cet6_seed.db")
KAOYAN_CACHE = os.path.join(CACHE_DIR, "kaoyan_jsons")
WORK_DB = "/tmp/omni_dict_cet_kaoyan_work.db"

os.makedirs(KAOYAN_CACHE, exist_ok=True)


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


def fetch_kaoyan_paths():
    url = "https://api.github.com/repos/LIziak112/structured-kaoyan-english/git/trees/HEAD?recursive=1"
    req = urllib.request.Request(url, headers={"User-Agent": "OmniWorkbench/1.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        tree = json.loads(r.read()).get("tree", [])
    return sorted(x["path"] for x in tree if x["path"].endswith(".json"))


def download_kaoyan_file(rel_path: str):
    safe = rel_path.replace("/", "_")
    local = os.path.join(KAOYAN_CACHE, safe)
    if os.path.exists(local) and os.path.getsize(local) > 100:
        with open(local, "r", encoding="utf-8") as f:
            return rel_path, json.load(f)
    url = f"https://raw.githubusercontent.com/LIziak112/structured-kaoyan-english/main/{rel_path}"
    req = urllib.request.Request(url, headers={"User-Agent": "OmniWorkbench/1.0"})
    with urllib.request.urlopen(req, timeout=30) as r:
        raw = r.read().decode("utf-8")
    with open(local, "w", encoding="utf-8") as f:
        f.write(raw)
    return rel_path, json.loads(raw)


def load_kaoyan_papers(vocab_set: set):
    paths = fetch_kaoyan_paths()
    print(f"  Fetching {len(paths)} Kaoyan JSON exam papers...", flush=True)
    papers = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=8) as pool:
        for res in pool.map(download_kaoyan_file, paths):
            papers.append(res)

    out = []
    q_total = 0
    for rel_path, obj in papers:
        year = obj.get("year", "")
        exam = obj.get("exam", "考研英语")
        label_base = f"{year} {exam}".strip()
        r_idx = 0
        for sec in obj.get("sections", []):
            for grp in sec.get("groups", []):
                gtype = grp.get("type", "")
                p_list = grp.get("passage") or []
                if isinstance(p_list, str):
                    p_list = [p_list]
                passage_raw = "\n\n".join(str(x).strip() for x in p_list if str(x).strip())
                if not passage_raw:
                    continue

                raw_qs = grp.get("questions") or []
                if gtype == "cloze":
                    # Replace {{1}}, {{2}}... with _ so interactive cloze UI binds cleanly
                    art = re.sub(r"\{\{\s*\d+\s*\}\}", "_", passage_raw)
                    blanks = len(re.findall(r"(?<!\w)_(?!\w)", art))
                    qs = []
                    for q in raw_qs:
                        opts_dict = q.get("options") or {}
                        opts = [str(opts_dict.get(k, "")).strip() for k in ("A", "B", "C", "D") if k in opts_dict]
                        ans = str(q.get("answer") or "").strip().upper()
                        exp = str(q.get("explanation") or "").strip()
                        if len(opts) >= 2 and ans in ("A", "B", "C", "D"):
                            qs.append({
                                "options": opts,
                                "answer": ans,
                                **({"explanation": exp} if exp else {}),
                            })
                    if qs and blanks == len(qs):
                        q_total += len(qs)
                        out.append((
                            f"[{label_base}] 完形填空 ({len(qs)}空)",
                            "Kaoyan",
                            "cloze",
                            f"{label_base} · 完形填空",
                            extract_target_words(art, vocab_set),
                            art,
                            "",
                            json.dumps(qs, ensure_ascii=False),
                        ))
                elif gtype == "reading":
                    r_idx += 1
                    qs = []
                    for q in raw_qs:
                        stem = str(q.get("stem") or q.get("question") or "").strip()
                        opts_dict = q.get("options") or {}
                        opts = [str(opts_dict.get(k, "")).strip() for k in ("A", "B", "C", "D") if k in opts_dict]
                        ans = str(q.get("answer") or "").strip().upper()
                        exp = str(q.get("explanation") or "").strip()
                        if stem and len(opts) >= 2 and ans in ("A", "B", "C", "D"):
                            qs.append({
                                "question": stem,
                                "options": opts,
                                "answer": ans,
                                **({"explanation": exp} if exp else {}),
                            })
                    if qs:
                        q_total += len(qs)
                        out.append((
                            f"[{label_base}] 阅读 Text {r_idx}",
                            "Kaoyan",
                            "reading",
                            f"{label_base} · 阅读理解",
                            extract_target_words(passage_raw, vocab_set),
                            passage_raw,
                            "",
                            json.dumps(qs, ensure_ascii=False),
                        ))
                elif gtype in ("translation_full", "translation_sentences"):
                    zh_parts = [str(q.get("answer") or "").strip() for q in raw_qs if str(q.get("answer") or "").strip()]
                    if zh_parts:
                        out.append((
                            f"[{label_base}] 英译汉精读",
                            "Kaoyan",
                            "bilingual",
                            f"{label_base} · 翻译双语",
                            extract_target_words(passage_raw, vocab_set),
                            passage_raw,
                            "\n\n".join(zh_parts),
                            "[]",
                        ))
    print(f"    -> Kaoyan (1998-2025): {len(out)} passages/sets, {q_total} MCQ/cloze questions", flush=True)
    return out


def load_cet_papers(vocab_set: set):
    if not os.path.exists(CET_SEED_DB):
        print(f"  [WARN] Missing {CET_SEED_DB}, skipping CET4/CET6", flush=True)
        return []

    conn = sqlite3.connect(f"file:{CET_SEED_DB}?mode=ro", uri=True)
    papers = conn.execute("SELECT id, paper_key, year, month, set_no, exam_type FROM papers WHERE exam_type IN ('cet4', 'cet6')").fetchall()

    out = []
    q_total = 0
    for pid, pkey, year, month, set_no, etype in papers:
        source = "CET6" if etype == "cet6" else "CET4"
        tag_zh = "六级 CET6" if etype == "cet6" else "四级 CET4"
        clean_key = pkey.replace("cet4-", "").replace("cet6-", "")
        paper_label = f"{clean_key}"

        mats = conn.execute(
            "SELECT section, content_json FROM section_materials WHERE paper_id = ? AND section IN ('reading1', 'reading2', 'cloze', 'matching')",
            (pid,),
        ).fetchall()

        for sec, cjson_str in mats:
            try:
                cjson = json.loads(cjson_str or "{}")
            except Exception:
                continue

            q_rows = conn.execute(
                "SELECT q_no, stem, options_json, answer, analysis_text FROM questions "
                "WHERE paper_id = ? AND section = ? ORDER BY q_no ASC, q_index ASC",
                (pid, sec),
            ).fetchall()
            if not q_rows:
                continue

            if sec in ("reading1", "reading2"):
                passage = (cjson.get("passage") or "").strip()
                if not passage:
                    continue
                qs = []
                for q_no, stem, ojson, ans, exp in q_rows:
                    try:
                        opts_raw = json.loads(ojson or "[]")
                        opts = [str(o.get("text", "")).strip() for o in opts_raw]
                    except Exception:
                        opts = []
                    ans_u = str(ans or "").strip().upper()
                    if stem and len(opts) >= 2 and ans_u in ("A", "B", "C", "D"):
                        qs.append({
                            "question": stem.strip(),
                            "options": opts[:4],
                            "answer": ans_u,
                            **({"explanation": exp.strip()} if exp and exp.strip() else {}),
                        })
                if qs:
                    idx_lbl = "Passage 1" if sec == "reading1" else "Passage 2"
                    q_total += len(qs)
                    out.append((
                        f"[{tag_zh} {paper_label}] 仔细阅读 {idx_lbl}",
                        source,
                        "reading",
                        f"{tag_zh} · {paper_label}",
                        extract_target_words(passage, vocab_set),
                        passage,
                        "",
                        json.dumps(qs, ensure_ascii=False),
                    ))
            elif sec == "cloze":
                passage_raw = (cjson.get("passage") or "").strip()
                if not passage_raw:
                    continue
                # Normalize __(26)__ or __26__ to _
                art = re.sub(r"_{2,}\s*\(?\s*\d+\s*\)?\s*_{2,}|_{2,}\d+_{2,}", "_", passage_raw)
                blanks = len(re.findall(r"(?<!\w)_(?!\w)", art))
                qs = []
                valid_letters = list("ABCDEFGHIJKLMNO")
                for q_no, stem, ojson, ans, exp in q_rows:
                    try:
                        opts_raw = json.loads(ojson or "[]")
                        opts = [str(o.get("text", "")).strip() for o in opts_raw]
                    except Exception:
                        opts = []
                    ans_u = str(ans or "").strip().upper()
                    if len(opts) >= 4 and ans_u in valid_letters[:len(opts)]:
                        qs.append({
                            "options": opts[:15],
                            "answer": ans_u,
                            **({"explanation": exp.strip()} if exp and exp.strip() else {}),
                        })
                if qs and blanks == len(qs):
                    q_total += len(qs)
                    out.append((
                        f"[{tag_zh} {paper_label}] 选词填空 ({len(qs)}空)",
                        source,
                        "cloze",
                        f"{tag_zh} · 选词填空",
                        extract_target_words(art, vocab_set),
                        art,
                        "",
                        json.dumps(qs, ensure_ascii=False),
                    ))
            elif sec == "matching":
                title_m = (cjson.get("title") or "").strip()
                paras = cjson.get("paragraphs") or []
                if not paras:
                    continue
                labels = [str(p.get("label") or "").strip().upper() for p in paras if p.get("label") and p.get("text")]
                passage = "\n\n".join(f"[{p['label']}] {p['text'].strip()}" for p in paras if p.get("label") and p.get("text"))
                if not passage or not labels:
                    continue
                opts = [f"段落 [{lbl}]" for lbl in labels[:15]]
                qs = []
                for q_no, stem, ojson, ans, exp in q_rows:
                    ans_u = str(ans or "").strip().upper()
                    if stem and ans_u in labels[:15]:
                        # Map answer letter to its index A, B, C... in opts
                        ans_code = list("ABCDEFGHIJKLMNO")[labels[:15].index(ans_u)]
                        qs.append({
                            "question": stem.strip(),
                            "options": opts,
                            "answer": ans_code,
                            **({"explanation": f"对应原段落 [{ans_u}]\n" + (exp.strip() if exp else "")}),
                        })
                if qs:
                    q_total += len(qs)
                    t_show = title_m if title_m else f"长篇信息匹配 ({len(qs)}题)"
                    out.append((
                        f"[{tag_zh} {paper_label}] {t_show}",
                        source,
                        "reading",
                        f"{tag_zh} · 长篇匹配",
                        extract_target_words(passage, vocab_set),
                        passage,
                        "",
                        json.dumps(qs, ensure_ascii=False),
                    ))

    conn.close()
    cet4_cnt = sum(1 for r in out if r[1] == "CET4")
    cet6_cnt = sum(1 for r in out if r[1] == "CET6")
    print(f"    -> CET4={cet4_cnt}, CET6={cet6_cnt} passages/sets, {q_total} questions", flush=True)
    return out


def main():
    if not os.path.exists(DB_TARGET):
        raise FileNotFoundError(f"Missing DB: {DB_TARGET}")

    print(f"Copying {DB_TARGET} -> {WORK_DB}...", flush=True)
    shutil.copy2(DB_TARGET, WORK_DB)

    conn = sqlite3.connect(WORK_DB)
    cur = conn.cursor()
    vocab_set = {row[0] for row in cur.execute("SELECT word FROM words;").fetchall() if row[0]}

    # Replace any existing Kaoyan / CET4 / CET6 rows idempotently
    cur.execute("DELETE FROM reading_articles WHERE source IN ('Kaoyan', 'CET4', 'CET6');")
    conn.commit()

    rows = []
    print("[1/2] Importing Kaoyan 1998-2025 (44 full papers)...", flush=True)
    rows.extend(load_kaoyan_papers(vocab_set))
    print("[2/2] Importing CET4 & CET6 (122 papers)...", flush=True)
    rows.extend(load_cet_papers(vocab_set))

    cur.executemany(
        """
        INSERT INTO reading_articles
          (title, source, kind, level, target_words, passage_en, passage_zh, questions_json)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?);
        """,
        rows,
    )
    print("Rebuilding FTS5 index...", flush=True)
    cur.execute("INSERT INTO reading_fts(reading_fts) VALUES('rebuild');")
    conn.commit()
    cur.execute("PRAGMA wal_checkpoint(TRUNCATE);")
    cur.execute("PRAGMA journal_mode=DELETE;")
    conn.commit()

    total_rd = cur.execute("SELECT COUNT(*) FROM reading_articles;").fetchone()[0]
    by_src = cur.execute("SELECT source, COUNT(*) FROM reading_articles GROUP BY source ORDER BY COUNT(*) DESC;").fetchall()
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
    print(f"Done! Total reading_articles={total_rd} | DB size={size_mb:.2f} MB", flush=True)
    print("Breakdown by source:", by_src, flush=True)


if __name__ == "__main__":
    main()
