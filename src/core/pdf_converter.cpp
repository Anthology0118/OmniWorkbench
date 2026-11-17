#include "pdf_converter.hpp"

#include "docx_converter.hpp"
#include "markdown.hpp"
#include "platform_utils.hpp"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace omni {

namespace fs = std::filesystem;

namespace {

std::string pdf_escape_ascii(const std::string& s) {
    std::string out;
    for (unsigned char c : s) {
        if (c == '(' || c == ')' || c == '\\') {
            out.push_back('\\');
            out.push_back(static_cast<char>(c));
        } else if (c >= 32 && c < 127) {
            out.push_back(static_cast<char>(c));
        } else {
            out.push_back('?');
        }
    }
    return out;
}

bool write_minimal_pdf_fallback(const MdDocument& doc, const std::string& out_pdf_path, std::string* err) {
    std::vector<std::string> lines;
    for (const auto& blk : doc.blocks) {
        std::string text;
        if (blk.kind == MdBlock::Kind::Heading) {
            text = std::string(static_cast<size_t>(blk.level), '#') + " " + blk.raw;
        } else if (blk.kind == MdBlock::Kind::MathBlock || blk.kind == MdBlock::Kind::CodeBlock) {
            text = blk.raw;
        } else if (blk.kind == MdBlock::Kind::Table) {
            for (const auto& row : blk.table_rows) {
                std::string rline;
                for (size_t i = 0; i < row.size(); ++i) {
                    if (i > 0) rline += " | ";
                    for (const auto& sp : row[i]) rline += sp.text;
                }
                lines.push_back(rline);
            }
            continue;
        } else if (blk.kind == MdBlock::Kind::Hr) {
            lines.push_back("----------------------------------------");
            continue;
        } else {
            text = blk.raw;
        }
        std::istringstream iss(text);
        std::string l;
        while (std::getline(iss, l)) {
            lines.push_back(l);
        }
    }

    if (lines.empty()) lines.push_back("OmniWorkbench Document");

    // Build basic A4 PDF 1.4 stream (595 x 842 pt)
    std::ostringstream stream;
    stream << "BT\n/F1 11 Tf\n50 790 Td\n16 TL\n";
    int y_lines = 0;
    for (const auto& l : lines) {
        if (y_lines >= 45) break;
        stream << "(" << pdf_escape_ascii(l) << ") Tj T*\n";
        ++y_lines;
    }
    stream << "ET\n";
    std::string stream_str = stream.str();

    std::vector<std::string> objs;
    objs.push_back("1 0 obj\n<< /Type /Catalog /Pages 2 0 R >>\nendobj\n");
    objs.push_back("2 0 obj\n<< /Type /Pages /Kids [3 0 R] /Count 1 >>\nendobj\n");
    objs.push_back("3 0 obj\n<< /Type /Page /Parent 2 0 R /MediaBox [0 0 595 842] /Contents 4 0 R /Resources << /Font << /F1 5 0 R >> >> >>\nendobj\n");
    objs.push_back("4 0 obj\n<< /Length " + std::to_string(stream_str.size()) + " >>\nstream\n" + stream_str + "endstream\nendobj\n");
    objs.push_back("5 0 obj\n<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\nendobj\n");

    std::ostringstream pdf;
    pdf << "%PDF-1.4\n";
    std::vector<size_t> offsets;
    offsets.push_back(0);
    for (const auto& o : objs) {
        offsets.push_back(static_cast<size_t>(pdf.tellp()));
        pdf << o;
    }
    size_t xref_pos = static_cast<size_t>(pdf.tellp());
    pdf << "xref\n0 " << (objs.size() + 1) << "\n";
    pdf << "0000000000 65535 f \n";
    for (size_t i = 1; i <= objs.size(); ++i) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%010zu 00000 n \n", offsets[i]);
        pdf << buf;
    }
    pdf << "trailer\n<< /Size " << (objs.size() + 1) << " /Root 1 0 R >>\nstartxref\n"
        << xref_pos << "\n%%EOF\n";

    return write_file_utf8(out_pdf_path, pdf.str(), err);
}

bool is_windows_binary_on_wsl(const std::string& bin) {
#ifdef _WIN32
    (void)bin;
    return false;
#else
    return bin.rfind("/mnt/", 0) == 0 && bin.size() >= 4 && bin.substr(bin.size() - 4) == ".exe";
#endif
}

} // namespace

bool markdown_to_pdf(const std::string& md,
                     const std::string& out_pdf_path,
                     const std::string& title,
                     std::string* err) {
    MdDocument doc = parse_markdown(md);
    std::string html = render_markdown_to_html(doc, true, title);

    std::error_code ec;
    fs::path out_p = fs::absolute(utf8_to_path(out_pdf_path), ec);
    if (out_p.has_parent_path()) {
        fs::create_directories(out_p.parent_path(), ec);
    }

    std::string browser = find_chromium_or_edge();
    if (!browser.empty()) {
        auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        std::string html_tmp_dir = get_temp_dir();
        if (is_windows_binary_on_wsl(browser) && fs::exists("/mnt/c/Windows/Temp", ec)) {
            html_tmp_dir = "/mnt/c/Windows/Temp";
        }
        fs::path html_path = utf8_to_path(html_tmp_dir) / ("omni_pdf_" + std::to_string(stamp) + ".html");
        fs::path pdf_work_path = out_p;
        bool need_copy_back = false;

        if (is_windows_binary_on_wsl(browser) && path_to_utf8(out_p).rfind("/mnt/", 0) != 0) {
            pdf_work_path = utf8_to_path(html_tmp_dir) / ("omni_pdf_" + std::to_string(stamp) + ".pdf");
            need_copy_back = true;
        }

        if (write_file_utf8(path_to_utf8(html_path), html)) {
            std::string browser_html_arg = to_win_path_if_wsl(path_to_utf8(html_path));
            std::string browser_pdf_arg = to_win_path_if_wsl(path_to_utf8(pdf_work_path));
            if (is_windows_binary_on_wsl(browser) ||
#ifdef _WIN32
                true
#else
                false
#endif
            ) {
                for (char& c : browser_html_arg) if (c == '\\') c = '/';
                browser_html_arg = "file:///" + browser_html_arg;
            } else {
                browser_html_arg = "file://" + browser_html_arg;
            }

            std::vector<std::string> args = {
                "--headless=new",
                "--disable-gpu",
                "--no-sandbox",
                "--no-pdf-header-footer",
                "--print-to-pdf=" + browser_pdf_arg,
                browser_html_arg
            };

            run_command_silent(browser, args);
            fs::remove(html_path, ec);

            if (fs::exists(pdf_work_path, ec) && fs::file_size(pdf_work_path, ec) > 0) {
                if (need_copy_back) {
                    fs::copy_file(pdf_work_path, out_p, fs::copy_options::overwrite_existing, ec);
                    fs::remove(pdf_work_path, ec);
                }
                if (fs::exists(out_p, ec) && fs::file_size(out_p, ec) > 0) {
                    return true;
                }
            }
        }
    }

    return write_minimal_pdf_fallback(doc, path_to_utf8(out_p), err);
}

bool docx_to_pdf(const std::string& docx_path,
                 const std::string& out_pdf_path,
                 std::string* err) {
    std::string md;
    if (!docx_to_markdown(docx_path, md, err)) {
        return false;
    }
    fs::path p = utf8_to_path(docx_path);
    std::string title = path_to_utf8(p.stem());
    return markdown_to_pdf(md, out_pdf_path, title, err);
}

} // namespace omni
