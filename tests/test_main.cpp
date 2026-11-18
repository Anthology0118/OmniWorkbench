#include "../src/core/config.hpp"
#include "../src/core/dict_manager.hpp"
#include "../src/core/docx_converter.hpp"
#include "../src/core/ds_manager.hpp"
#include "../src/core/json.hpp"
#include "../src/core/markdown.hpp"
#include "../src/core/pdf_converter.hpp"
#include "../src/core/platform_utils.hpp"
#include "../src/core/vault_manager.hpp"
#include "../src/core/xml_doc.hpp"
#include "../src/core/zip_archive.hpp"
#include "../src/generated/embedded_ui.hpp"
#include "../src/server/app_server.hpp"

#include <cassert>
#include <filesystem>
#include <iostream>
#include <string>

#ifndef _WIN32
  #include <arpa/inet.h>
  #include <netinet/in.h>
  #include <sys/socket.h>
  #include <unistd.h>
#endif

namespace fs = std::filesystem;

static void test_json() {
    omni::JsonValue v;
    std::string err;
    bool ok = omni::JsonValue::parse(R"({"name":"Omni测试","n":42,"arr":[true,null,"x"]})", v, &err);
    assert(ok && err.empty());
    assert(v.get("name").as_string() == "Omni测试");
    assert(v.get("n").as_int() == 42);
    assert(v.get("arr").arr_val.size() == 3);
    assert(v.get("arr").arr_val[0].as_bool() == true);

    std::string dumped = v.dump();
    omni::JsonValue v2;
    assert(omni::JsonValue::parse(dumped, v2));
    assert(v2.get("name").as_string() == "Omni测试");
    std::cout << "[PASS] test_json\n";
}

static void test_zip_and_xml() {
    omni::ZipArchive z;
    z.set_entry("hello.txt", "Hello OmniWorkbench 中文测试!");
    z.set_entry("word/document.xml", "<w:document><w:body><w:p><w:r><w:t>Test</w:t></w:r></w:p></w:body></w:document>");

    std::string bytes;
    assert(z.save_to_memory(bytes));

    omni::ZipArchive z2;
    assert(z2.load_from_memory(bytes));
    std::string out;
    assert(z2.get_entry("hello.txt", out) && out == "Hello OmniWorkbench 中文测试!");

    std::string xml_out;
    assert(z2.get_entry("word/document.xml", xml_out));
    auto dom = omni::parse_xml(xml_out);
    assert(dom != nullptr);
    auto doc = dom->first_child("document");
    assert(doc != nullptr);
    assert(doc->inner_text() == "Test");
    std::cout << "[PASS] test_zip_and_xml\n";
}

static void test_md_docx_roundtrip_and_pdf(const fs::path& tmp_dir) {
    std::string md =
        "# 项目规划标题\n\n"
        "###### 六级标题黑体\n\n"
        "<!-- wp:align=justify;line=min:23.0pt;before=6.0;after=12.0;indent=2.0 -->\n"
        "这是一个包含 **粗体**、*斜体*、***粗斜体***、<u>下划线</u>、<sup>2</sup>、<sub>i</sub>、<mark style=\"background-color:#FEF08A\">高亮</mark>、<span style=\"color:#DC2626\">红字</span>、`代码` 和 [[双链笔记]] 的段落。<br/>第二行软换行。\n\n"
        "## 公式与表格\n\n"
        "$$f(x) = \\frac{1}{\\sqrt{2\\pi}} + \\sum_{i=1}^{n} x_i^2 + \\begin{pmatrix} a & b \\\\ c & d \\end{pmatrix}$$\n\n"
        "> 引用说明段落。\n\n"
        "- 第一项列表\n"
        "  - 子级缩进列表\n"
        "1. 有序第一项\n"
        "- [x] 已完成任务\n"
        "- [ ] 待办任务\n\n"
        "```python\nprint('hello wps')\n```\n\n"
        "```figma\n{\"width\":680,\"height\":360,\"title\":\"测试算法框\",\"nodes\":[{\"id\":\"n1\",\"shape\":\"pill\",\"x\":40,\"y\":40,\"w\":120,\"h\":44,\"fill\":\"#EFF6FF\",\"stroke\":\"#2563EB\",\"label\":\"Start\"},{\"id\":\"n2\",\"shape\":\"rect\",\"x\":240,\"y\":40,\"w\":140,\"h\":48,\"fill\":\"#ECFDF5\",\"stroke\":\"#059669\",\"label\":\"Process\"}],\"edges\":[{\"from\":\"n1\",\"to\":\"n2\",\"label\":\"next\"}]}\n```\n\n"
        "![dot](data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg==)\n\n"
        "<!-- pagebreak -->\n\n"
        "| 指标 | 数值 | 备注 |\n"
        "| :--- | :---: | ---: |\n"
        "| 精度 | 96.5% | 合格 |\n";

    omni::MdDocument ast = omni::parse_markdown(md);
    assert(!ast.blocks.empty());
    std::string html = omni::render_markdown_to_html(ast, true, "测试文档");
    assert(html.find("omni-h1") != std::string::npos);
    assert(html.find("omni-h6") != std::string::npos);
    assert(html.find("omni-table") != std::string::npos);
    assert(html.find("omni-wikilink") != std::string::npos);
    assert(html.find("omni-figma-block") != std::string::npos);
    assert(html.find("omni-mmatrix") != std::string::npos);
    assert(html.find("宋体") != std::string::npos);
    assert(html.find("黑体") != std::string::npos);

    fs::path docx_path = tmp_dir / omni::utf8_to_path("生词沉淀/roundtrip_中文.docx");
    std::string docx_utf8 = omni::path_to_utf8(docx_path);
    std::string err;
    assert(omni::markdown_to_docx(md, docx_utf8, &err));
    assert(fs::exists(docx_path) && fs::file_size(docx_path) > 1000);

    // Inspect OOXML package entries directly for WPS compatibility
    omni::ZipArchive pkg;
    assert(pkg.load_from_file(docx_utf8));
    std::string styles_xml, numbering_xml, doc_xml;
    assert(pkg.get_entry("word/styles.xml", styles_xml));
    assert(pkg.get_entry("word/numbering.xml", numbering_xml));
    assert(pkg.get_entry("word/document.xml", doc_xml));
    assert(styles_xml.find("宋体") != std::string::npos);
    assert(styles_xml.find("黑体") != std::string::npos);
    assert(styles_xml.find("楷体") != std::string::npos);
    assert(styles_xml.find("Times New Roman") != std::string::npos);
    assert(styles_xml.find("Heading6") != std::string::npos);
    assert(doc_xml.find("<w:numPr>") != std::string::npos);
    assert(doc_xml.find("<m:m>") != std::string::npos);
    assert(doc_xml.find("OMNI_FIGMA:") != std::string::npos);
    assert(doc_xml.find("<w:drawing>") != std::string::npos);
    assert(doc_xml.find("w:line=\"460\" w:lineRule=\"atLeast\"") != std::string::npos);
    assert(doc_xml.find("<w:br w:type=\"page\"/>") != std::string::npos);

    std::string extracted_md;
    assert(omni::docx_to_markdown(docx_utf8, extracted_md, &err));
    assert(extracted_md.find("# 项目规划标题") != std::string::npos);
    assert(extracted_md.find("###### 六级标题黑体") != std::string::npos);
    assert(extracted_md.find("line=min:23.0pt") != std::string::npos);
    assert(extracted_md.find("before=6.0") != std::string::npos);
    assert(extracted_md.find("after=12.0") != std::string::npos);
    assert(extracted_md.find("**粗体**") != std::string::npos);
    assert(extracted_md.find("***粗斜体***") != std::string::npos);
    assert(extracted_md.find("<u>下划线</u>") != std::string::npos);
    assert(extracted_md.find("## 公式与表格") != std::string::npos);
    assert(extracted_md.find("| 指标 | 数值 | 备注 |") != std::string::npos);
    assert(extracted_md.find(":---:") != std::string::npos);
    assert(extracted_md.find("---:") != std::string::npos);
    assert(extracted_md.find("```python") != std::string::npos);
    assert(extracted_md.find("```figma") != std::string::npos);
    assert(extracted_md.find("data:image/png;base64,") != std::string::npos);
    assert(extracted_md.find("<!-- pagebreak -->") != std::string::npos);
    assert(extracted_md.find("- [x] 已完成任务") != std::string::npos);
    assert(extracted_md.find("\\frac") != std::string::npos);
    assert(extracted_md.find("\\begin{pmatrix}") != std::string::npos);

    fs::path pdf_path = tmp_dir / "from_md.pdf";
    assert(omni::markdown_to_pdf(md, pdf_path.string(), "测试文档", &err));
    assert(fs::exists(pdf_path) && fs::file_size(pdf_path) > 200);
    std::string pdf_bytes;
    assert(omni::read_file_utf8(pdf_path.string(), pdf_bytes));
    assert(pdf_bytes.rfind("%PDF-", 0) == 0);

    fs::path pdf_from_docx = tmp_dir / "from_docx.pdf";
    assert(omni::docx_to_pdf(docx_utf8, pdf_from_docx.string(), &err));
    assert(fs::exists(pdf_from_docx) && fs::file_size(pdf_from_docx) > 200);

    std::cout << "[PASS] test_md_docx_roundtrip_and_pdf\n";
}

static void test_vault_and_dict(const fs::path& tmp_dir) {
    fs::path vault_dir = tmp_dir / "Vault";
    omni::VaultManager vm(vault_dir.string());
    vm.ensure_initialized();

    auto tree = vm.scan_tree();
    assert(!tree.get("categories").arr_val.empty());
    assert(!tree.get("files").arr_val.empty());

    std::string resolved = vm.resolve_wikilink("Vocabulary");
    assert(!resolved.empty());

    assert(vm.append_vocabulary_entry("orthogonal", "/ɔːˈθɒɡənl/", "adj. 正交的", "线性代数核心词"));
    omni::VaultFileContent vfc;
    assert(vm.open_document(resolved, vfc));
    assert(vfc.markdown.find("orthogonal") != std::string::npos);

    // Unbound VaultManager stays empty and never seeds defaults
    omni::VaultManager unbound_vm("");
    assert(unbound_vm.root().empty());
    assert(!unbound_vm.root_exists());
    assert(unbound_vm.compute_signature() == "UNBOUND");

    // External deletion / rename of bound directory is detected accurately
    std::string sig_before = vm.compute_signature();
    assert(sig_before.rfind("OK:", 0) == 0);
    std::error_code del_ec;
    fs::remove_all(vault_dir, del_ec);
    assert(!vm.root_exists());
    assert(vm.compute_signature().rfind("MISSING:", 0) == 0);
    auto missing_tree = vm.scan_tree();
    assert(missing_tree.get("bound").as_bool(false) == true);
    assert(missing_tree.get("root_exists").as_bool(true) == false);

    omni::DictManager dm((tmp_dir / "test_dict.db").string());
    assert(dm.init());
    auto res = dm.search("insight");
    assert(!res.empty() && res[0].word == "insight");
    assert(dm.set_marked("paradigm", true));
    omni::DictEntry p;
    assert(dm.get_word("paradigm", p) && p.marked);
    assert(dm.save_note("paradigm", "Updated test note"));
    assert(dm.get_word("paradigm", p) && p.note == "Updated test note");

    // Test importing kajweb/dict NDJSON
    fs::path kajweb_json = tmp_dir / "CET6_sample.json";
    omni::write_file_utf8(kajweb_json.string(),
        "{\"headWord\":\"resilient\",\"bookId\":\"CET6_2\",\"content\":{\"word\":{\"content\":{\"usphone\":\"rɪˈzɪliənt\",\"trans\":[{\"pos\":\"adj\",\"tranCn\":\"有弹性的；能迅速恢复的\"}],\"sentence\":{\"sentences\":[{\"sContent\":\"Systems must remain resilient.\",\"sCn\":\"系统必须保持韧性。\"}]},\"phrase\":{\"phrases\":[{\"pContent\":\"resilient architecture\",\"pCn\":\"弹性架构\"}]}}}}}\n");
    int imp_count = 0;
    assert(dm.import_dictionary_file(kajweb_json.string(), "", imp_count) && imp_count == 1);
    auto cet6_hits = dm.search("resilient", 10, "CET6");
    assert(cet6_hits.size() == 1 && cet6_hits[0].ipa == "/rɪˈzɪliənt/");

    // Test importing KyleBing/english-vocabulary JSON array
    fs::path kyle_json = tmp_dir / "Kyle_IELTS.json";
    omni::write_file_utf8(kyle_json.string(),
        "[{\"word\":\"ephemeral\",\"translations\":[{\"type\":\"adj\",\"translation\":\"短暂的；瞬息的\"}],\"phrases\":[{\"phrase\":\"ephemeral nature\",\"translation\":\"短暂本质\"}]}]");
    assert(dm.import_dictionary_file(kyle_json.string(), "IELTS", imp_count) && imp_count == 1);
    auto ielts_hits = dm.search("ephemeral", 10, "IELTS");
    assert(ielts_hits.size() == 1 && ielts_hits[0].pos.find("短暂的") != std::string::npos);

    // Test importing skywind3000/ECDICT CSV
    fs::path ecdict_csv = tmp_dir / "ecdict.csv";
    omni::write_file_utf8(ecdict_csv.string(),
        "word,phonetic,definition,translation,pos,collins,oxford,tag,bnc,frq,exchange,detail,audio\n"
        "\"serendipity\",\"ˌserənˈdɪpəti\",\"good luck\",\"n. 意外发现珍奇事物的本领\",n,1,0,\"gre toefl\",12000,14000,,,\n");
    assert(dm.import_dictionary_file(ecdict_csv.string(), "", imp_count) && imp_count == 1);
    auto gre_hits = dm.search("serendipity", 10, "GRE");
    assert(gre_hits.size() == 1 && gre_hits[0].pos.find("意外发现") != std::string::npos);

    // Test ECDICT exchange lemma reverse lookup (went -> go)
    auto went_hits = dm.search("went", 10);
    assert(!went_hits.empty() && went_hits[0].word == "go");
    assert(went_hits[0].matched_from.find("went") != std::string::npos);

    // Test phonetic cluster search and kajweb .zip import
    auto ph_hits = dm.search_by_phonetic("igh", "/aɪ/", 20);
    assert(!ph_hits.empty());
    fs::path kajweb_zip = tmp_dir / "IELTS_sample.zip";
    omni::ZipArchive kz;
    kz.set_entry("IELTS_sample.json",
        "{\"headWord\":\"ubiquity\",\"bookId\":\"IELTS_2\",\"content\":{\"word\":{\"content\":{\"usphone\":\"juːˈbɪkwəti\",\"ukphone\":\"juːˈbɪkwɪti\",\"trans\":[{\"pos\":\"n\",\"tranCn\":\"无处不在\"}],\"remMethod\":{\"val\":\"ubiqu(到处)+ity\"},\"sentence\":{\"sentences\":[{\"sContent\":\"The ubiquity of smartphones is clear.\",\"sCn\":\"智能手机的普及显而易见。\"}]},\"phrase\":{\"phrases\":[{\"pContent\":\"digital ubiquity\",\"pCn\":\"数字普及\"}]}}}}}\n");
    assert(kz.save_to_file(kajweb_zip.string()));
    assert(dm.import_dictionary_file(kajweb_zip.string(), "", imp_count) && imp_count == 1);
    auto zip_hits = dm.search("ubiquity", 10, "IELTS");
    assert(zip_hits.size() == 1);
    assert(zip_hits[0].rem_method.find("ubiqu") != std::string::npos);
    assert(!zip_hits[0].sentences.empty());
    assert(!zip_hits[0].phrases.empty());

    // Test reading_articles, stats, similar_words & pagination
    auto race_hits = dm.search_readings("apply", "RACE", "", 10);
    assert(!race_hits.empty());
    int r_arts = 0, r_qs = 0;
    dm.reading_stats("apply", "", "", r_arts, r_qs);
    assert(r_arts >= 1 && r_qs >= 2);
    omni::ReadingArticle full_rd;
    assert(dm.get_reading(race_hits[0].id, full_rd));
    assert(full_rd.question_count >= 1);
    omni::ReadingWordMark rwm{full_rd.id, "apply", "apply", "yellow", "v. 应用", "Researchers often apply a simple heuristic."};
    assert(dm.set_reading_mark(rwm));
    auto rmarks = dm.get_reading_marks(full_rd.id);
    assert(rmarks.size() == 1 && rmarks[0].color == "yellow");
    int sim_total = 0;
    auto sim_hits = dm.similar_words("heuristic", "", 5, 0, &sim_total);
    assert(sim_total >= 0);
    (void)sim_hits;

    std::cout << "[PASS] test_vault_and_dict\n";
}

#ifndef _WIN32
static std::string http_get(uint16_t port, const std::string& path) {
    int sock = ::socket(AF_INET, SOCK_STREAM, 0);
    assert(sock >= 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    assert(::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == 0);
    std::string req = "GET " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
    assert(::send(sock, req.data(), req.size(), 0) == static_cast<ssize_t>(req.size()));
    std::string resp;
    char buf[4096];
    ssize_t n;
    while ((n = ::recv(sock, buf, sizeof(buf), 0)) > 0) resp.append(buf, static_cast<size_t>(n));
    ::close(sock);
    return resp;
}
#endif

static void test_http_server() {
#ifndef _WIN32
    omni::AppServer srv(omni::get_embedded_ui_html());
    assert(srv.start(0));
    uint16_t port = srv.port();
    assert(port > 0);

    std::string html_resp = http_get(port, "/");
    assert(html_resp.find("200 OK") != std::string::npos);
    assert(html_resp.find("OmniWorkbench") != std::string::npos);

    std::string state_resp = http_get(port, "/api/state");
    assert(state_resp.find("200 OK") != std::string::npos);
    assert(state_resp.find("\"ok\":true") != std::string::npos);

    std::string dict_resp = http_get(port, "/api/dict/search?q=heuristic&limit=200");
    assert(dict_resp.find("heuristic") != std::string::npos);
    assert(dict_resp.find("\"match_total\"") != std::string::npos);

    std::string sim_resp = http_get(port, "/api/dict/similar?w=heuristic&limit=20&offset=0");
    assert(sim_resp.find("200 OK") != std::string::npos);
    assert(sim_resp.find("\"match_total\"") != std::string::npos);

    std::string ph_resp = http_get(port, "/api/dict/by-phonetic?combo=d&ipa=/d/&limit=50&offset=0");
    assert(ph_resp.find("200 OK") != std::string::npos);
    assert(ph_resp.find("\"match_total\"") != std::string::npos);

    std::string rd_resp = http_get(port, "/api/reading/search?q=apply");
    assert(rd_resp.find("200 OK") != std::string::npos);
    assert(rd_resp.find("RACE") != std::string::npos);
    assert(rd_resp.find("\"match_questions\"") != std::string::npos);
    std::string rd_item = http_get(port, "/api/reading/item?id=1");
    assert(rd_item.find("200 OK") != std::string::npos);
    assert(rd_item.find("passageEn") != std::string::npos);

    std::string ds_cat = http_get(port, "/api/ds/catalog");
    assert(ds_cat.find("200 OK") != std::string::npos);
    assert(ds_cat.find("Dijkstra") != std::string::npos);
    std::string ds_les = http_get(port, "/api/ds/lesson?id=7.3A");
    assert(ds_les.find("200 OK") != std::string::npos);
    assert(ds_les.find("reverseList") != std::string::npos);

    srv.stop();
    std::cout << "[PASS] test_http_server\n";
#endif
}

static void test_ds_course(const fs::path& tmp_dir) {
    fs::path dbp = tmp_dir / "test_ds.db";
    omni::DsManager ds(dbp.string());
    assert(ds.init());
    assert(ds.total_lessons() >= 50);
    assert(ds.total_exercises() >= 55);

    auto cat = ds.get_catalog("", "");
    assert(cat.get("ok").as_bool(false));
    assert(cat.get("chapters").arr_val.size() >= 10);

    auto filt = ds.get_catalog("Dijkstra", "");
    assert(filt.get("chapters").arr_val.size() == 1);

    omni::DsLesson les;
    assert(ds.get_lesson("7.3A", les));
    assert(!les.tutorial_html.empty());
    assert(!les.exercises.empty());
    assert(ds.save_progress("7.3A", true, "三指针注意nxt保后继", les.exercises[0].id, "// saved", "PASS", true));

    // Verify reload persistence
    {
        omni::DsManager ds2(dbp.string());
        assert(ds2.init());
        omni::DsLesson les2;
        assert(ds2.get_lesson("7.3A", les2));
        assert(les2.mastered == true);
        assert(les2.note == "三指针注意nxt保后继");
        assert(les2.exercises[0].completed == true);
    }

    // If g++ is available in environment, verify solution compilation & grading
    if (!omni::DsManager::find_cpp_compiler().empty()) {
        auto rr = ds.run_and_grade(les.exercises[0].id, les.exercises[0].solution_code, "", true);
        assert(rr.compiled);
        assert(rr.all_passed);
        assert(rr.passed_count == rr.total_count);
    }

    std::cout << "[PASS] test_ds_course\n";
}

int main() {
    fs::path tmp = fs::temp_directory_path() / "omniworkbench_test_suite";
    std::error_code ec;
    fs::remove_all(tmp, ec);
    fs::create_directories(tmp, ec);

    test_json();
    test_zip_and_xml();
    test_md_docx_roundtrip_and_pdf(tmp);
    test_vault_and_dict(tmp);
    test_ds_course(tmp);
    test_http_server();

    fs::remove_all(tmp, ec);
    std::cout << "\nAll OmniWorkbench C++ tests PASSED!\n";
    return 0;
}
