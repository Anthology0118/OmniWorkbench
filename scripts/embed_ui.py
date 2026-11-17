#!/usr/bin/env python3
"""Generate C++ header embedding web/index.html (and assets/app_icon.png) as raw byte arrays."""
import pathlib
import sys


def emit_byte_func(lines, func_name, data):
    lines.append(f"inline std::string {func_name}() {{")
    lines.append("    static const unsigned char kBytes[] = {")
    for i in range(0, len(data), 16):
        chunk = data[i:i + 16]
        lines.append("        " + ", ".join(f"0x{b:02x}" for b in chunk) + ",")
    lines.extend([
        "    };",
        f"    return std::string(reinterpret_cast<const char*>(kBytes), {len(data)});",
        "}",
        "",
    ])


def main():
    if len(sys.argv) != 3:
        print("Usage: embed_ui.py <input.html> <output.hpp>", file=sys.stderr)
        sys.exit(1)
    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])
    dst.parent.mkdir(parents=True, exist_ok=True)
    html_data = src.read_bytes()

    root_dir = src.resolve().parent.parent
    icon_path = root_dir / "assets" / "app_icon.png"
    icon_data = icon_path.read_bytes() if icon_path.exists() else b""

    lines = [
        "#pragma once",
        "#include <string>",
        "",
        "namespace omni {",
    ]
    emit_byte_func(lines, "get_embedded_ui_html", html_data)
    emit_byte_func(lines, "get_embedded_icon_png", icon_data)
    lines.append("} // namespace omni")
    lines.append("")
    dst.write_text("\n".join(lines), encoding="utf-8")


if __name__ == "__main__":
    main()
