#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WIN_STAGE_WSL="/mnt/d/msys/tmp/omni_build"
GPP="/mnt/d/msys/ucrt64/bin/g++.exe"

python3 "$ROOT_DIR/scripts/embed_ui.py" "$ROOT_DIR/web/index.html" "$ROOT_DIR/src/generated/embedded_ui.hpp"

rm -rf "$WIN_STAGE_WSL"
mkdir -p "$WIN_STAGE_WSL/obj" "$ROOT_DIR/build/win"
cp -r "$ROOT_DIR/src" "$WIN_STAGE_WSL/"
cp -r "$ROOT_DIR/tests" "$WIN_STAGE_WSL/"

cd "$WIN_STAGE_WSL"

CORE_SRCS=(
  src/core/config.cpp
  src/core/dict_manager.cpp
  src/core/docx_converter.cpp
  src/core/json.cpp
  src/core/markdown.cpp
  src/core/pdf_converter.cpp
  src/core/platform_utils.cpp
  src/core/vault_manager.cpp
  src/core/xml_doc.cpp
  src/core/zip_archive.cpp
  src/server/app_server.cpp
)

CFLAGS=(-std=c++17 -O2 -Wall -Wextra -Isrc -DOMNI_USE_SQLITE3=1)
OBJS=()

for src in "${CORE_SRCS[@]}"; do
  obj="obj/$(basename "${src%.cpp}").o"
  echo "  [CC] $src"
  timeout 30 "$GPP" "${CFLAGS[@]}" -c "$src" -o "$obj" < /dev/null
  OBJS+=("$obj")
done

echo "  [CC] src/desktop/main.cpp"
timeout 30 "$GPP" "${CFLAGS[@]}" -c src/desktop/main.cpp -o obj/main.o < /dev/null

echo "  [LD] OmniWorkbench.exe"
timeout 30 "$GPP" -mwindows -static "${OBJS[@]}" obj/main.o -o OmniWorkbench.exe \
  -lsqlite3 -lz -lws2_32 -lole32 -lshell32 -lcomdlg32 -luuid -lpthread < /dev/null

echo "  [CC] tests/test_main.cpp"
timeout 30 "$GPP" "${CFLAGS[@]}" -c tests/test_main.cpp -o obj/test_main.o < /dev/null

echo "  [LD] omni_tests.exe"
timeout 30 "$GPP" -static "${OBJS[@]}" obj/test_main.o -o omni_tests.exe \
  -lsqlite3 -lz -lws2_32 -lole32 -lshell32 -lcomdlg32 -luuid -lpthread < /dev/null

echo "  [TEST] Running omni_tests.exe on Windows..."
timeout 30 ./omni_tests.exe < /dev/null

cp "$WIN_STAGE_WSL/OmniWorkbench.exe" "$ROOT_DIR/build/win/OmniWorkbench.exe"
ls -lh "$ROOT_DIR/build/win/OmniWorkbench.exe"
