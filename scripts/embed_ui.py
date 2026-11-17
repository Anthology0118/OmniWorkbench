#!/usr/bin/env python3
"""Generate C++ header embedding web/index.html as a raw byte array."""
import pathlib
import sys

def main():
    if len(sys.argv) != 3:
        print("Usage: embed_ui.py <input.html> <output.hpp>", file=sys.stderr)
        sys.exit(1)
    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])
    dst.parent.mkdir(parents=True, exist_ok=True)
    data = src.read_bytes()

    lines = [
        "#pragma once",
        "#include <string>",
        "",
        "namespace omni {",
        "inline std::string get_embedded_ui_html() {",
        "    static const unsigned char kHtmlBytes[] = {"
    ]
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("        " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.extend([
        "    };",
        f"    return std::string(reinterpret_cast<const char*>(kHtmlBytes), {len(data)});",
        "}",
        "} // namespace omni",
        ""
    ])
    dst.write_text("\n".join(lines), encoding="utf-8")

if __name__ == "__main__":
    main()
