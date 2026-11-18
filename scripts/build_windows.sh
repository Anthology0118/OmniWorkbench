#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
WIN_STAGE_WSL="/mnt/d/msys/tmp/omni_build"
UCRT_BIN="/mnt/d/msys/ucrt64/bin"
UCRT_INC="/mnt/d/msys/ucrt64/include"
UCRT_LIB="/mnt/d/msys/ucrt64/lib"
UCRT_INC_WIN="D:/msys/ucrt64/include"
UCRT_LIB_WIN="D:/msys/ucrt64/lib"
GPP="$UCRT_BIN/g++.exe"
WINDRES="$UCRT_BIN/windres.exe"
OBJDUMP="$UCRT_BIN/objdump.exe"
QT_INC="$UCRT_INC/qt6"
QT_INC_WIN="$UCRT_INC_WIN/qt6"

python3 "$ROOT_DIR/scripts/embed_ui.py" "$ROOT_DIR/web/index.html" "$ROOT_DIR/src/generated/embedded_ui.hpp"

rm -rf "$WIN_STAGE_WSL"
mkdir -p "$WIN_STAGE_WSL/obj" "$WIN_STAGE_WSL/qt_app" "$ROOT_DIR/build/win"
cp -r "$ROOT_DIR/src" "$WIN_STAGE_WSL/"
cp -r "$ROOT_DIR/tests" "$WIN_STAGE_WSL/"
if [ -d "$ROOT_DIR/assets" ]; then
  cp -r "$ROOT_DIR/assets" "$WIN_STAGE_WSL/"
fi

cd "$WIN_STAGE_WSL"

CORE_SRCS=(
  src/core/config.cpp
  src/core/dict_manager.cpp
  src/core/docx_converter.cpp
  src/core/ds_manager.cpp
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

RES_OBJ=()
if [ -f "assets/app.rc" ] && [ -x "$WINDRES" ]; then
  echo "  [RC] assets/app.rc"
  timeout 30 "$WINDRES" assets/app.rc -O coff -o obj/app_res.o < /dev/null
  RES_OBJ+=("obj/app_res.o")
fi

echo "  [CC] tests/test_main.cpp"
timeout 30 "$GPP" "${CFLAGS[@]}" -c tests/test_main.cpp -o obj/test_main.o < /dev/null

echo "  [LD] omni_tests.exe"
timeout 30 "$GPP" -static "${OBJS[@]}" obj/test_main.o -o omni_tests.exe \
  -lsqlite3 -lz -lws2_32 -lole32 -lshell32 -lcomdlg32 -luuid -lpthread < /dev/null

echo "  [TEST] Running omni_tests.exe on Windows..."
timeout 30 ./omni_tests.exe < /dev/null

if [ -d "$QT_INC/QtWidgets" ]; then
  echo "  [CC] src/desktop/qt_main.cpp (Pure Qt 6 Native)"
  QT_FLAGS=("${CFLAGS[@]}" "-I$QT_INC_WIN" "-I$QT_INC_WIN/QtWidgets" "-I$QT_INC_WIN/QtGui" "-I$QT_INC_WIN/QtCore")
  timeout 45 "$GPP" "${QT_FLAGS[@]}" -c src/desktop/qt_main.cpp -o obj/qt_main.o < /dev/null

  echo "  [LD] qt_app/OmniWorkbenchQt.exe"
  timeout 45 "$GPP" -mwindows "${OBJS[@]}" obj/qt_main.o "${RES_OBJ[@]}" -o qt_app/OmniWorkbenchQt.exe \
    "-L$UCRT_LIB_WIN" -lQt6Widgets -lQt6Gui -lQt6Core \
    -lsqlite3 -lz -lws2_32 -lole32 -lshell32 -lcomdlg32 -luuid -lpthread < /dev/null

  echo "  [DEPLOY] Gathering Qt 6 plugins & transitive UCRT64 DLLs..."
  QT_PLUGINS="/mnt/d/msys/ucrt64/share/qt6/plugins"
  mkdir -p qt_app/platforms qt_app/styles qt_app/imageformats
  [ -f "$QT_PLUGINS/platforms/qwindows.dll" ] && cp -f "$QT_PLUGINS/platforms/qwindows.dll" qt_app/platforms/
  [ -f "$QT_PLUGINS/styles/qmodernwindowsstyle.dll" ] && cp -f "$QT_PLUGINS/styles/qmodernwindowsstyle.dll" qt_app/styles/
  [ -d "$QT_PLUGINS/imageformats" ] && cp -f "$QT_PLUGINS/imageformats/"*.dll qt_app/imageformats/ 2>/dev/null || true
  [ -f "assets/app_icon.png" ] && cp -f "assets/app_icon.png" qt_app/app_icon.png
  printf '[Paths]\nPlugins = .\n' > qt_app/qt.conf

  # Recursively trace and copy every UCRT64 runtime DLL required by exe & plugins
  python3 - "$UCRT_BIN" "$OBJDUMP" "$WIN_STAGE_WSL/qt_app" <<'PY'
import os, re, shutil, subprocess, sys

ucrt_bin, objdump, qt_dir = sys.argv[1], sys.argv[2], sys.argv[3]
def to_win(p):
    return subprocess.check_output(["wslpath", "-w", p], text=True).strip()

queue = []
for root, _, files in os.walk(qt_dir):
    for f in files:
        if f.lower().endswith((".exe", ".dll")):
            queue.append(os.path.join(root, f))

seen = set()
copied = 0
while queue:
    target = queue.pop(0)
    r = subprocess.run([objdump, "-p", to_win(target)], capture_output=True, text=True)
    for m in re.finditer(r"DLL Name:\s*([^\s]+)", r.stdout):
        dll = m.group(1)
        low = dll.lower()
        if low in seen:
            continue
        seen.add(low)
        cand = os.path.join(ucrt_bin, dll)
        if os.path.exists(cand):
            dst = os.path.join(qt_dir, os.path.basename(cand))
            if not os.path.exists(dst):
                shutil.copy2(cand, dst)
                copied += 1
            queue.append(dst)
print(f"  [DEPLOY] Copied {copied} UCRT64/Qt6 runtime DLLs into qt_app/")
PY

  echo "  [CC] src/desktop/launcher.cpp"
  timeout 30 "$GPP" -std=c++17 -O2 -Wall -Wextra -c src/desktop/launcher.cpp -o obj/launcher.o < /dev/null

  echo "  [LD] OmniWorkbench.exe (Standalone Launcher)"
  timeout 30 "$GPP" -mwindows -municode -static obj/launcher.o "${RES_OBJ[@]}" -o OmniWorkbench.exe < /dev/null

  echo "  [TEST] Running Qt 6 Native UI --self-test on Windows (isolated System32 PATH)..."
  timeout 30 /mnt/c/Windows/System32/cmd.exe /c "set PATH=C:\Windows\System32;C:\Windows && D:\msys\tmp\omni_build\qt_app\OmniWorkbenchQt.exe --self-test" < /dev/null
  echo "[PASS] Qt6 native GUI isolated self-test exited 0"

  rm -rf "$ROOT_DIR/build/win/qt_app"
  cp -r "$WIN_STAGE_WSL/qt_app" "$ROOT_DIR/build/win/qt_app"
  cp -f "$WIN_STAGE_WSL/OmniWorkbench.exe" "$ROOT_DIR/build/win/OmniWorkbench.exe"

  # Also deploy qt_app runtime to %LOCALAPPDATA%\OmniWorkbench\qt_app so Desktop launcher works cleanly
  LOCAL_QT="/mnt/c/Users/34970/AppData/Local/OmniWorkbench/qt_app"
  mkdir -p "$(dirname "$LOCAL_QT")"
  rm -rf "$LOCAL_QT"
  cp -r "$WIN_STAGE_WSL/qt_app" "$LOCAL_QT"
else
  echo "  [WARN] Qt6 headers not found in $QT_INC; building fallback HTML desktop launcher"
  timeout 30 "$GPP" "${CFLAGS[@]}" -c src/desktop/main.cpp -o obj/main.o < /dev/null
  timeout 30 "$GPP" -mwindows -static "${OBJS[@]}" obj/main.o "${RES_OBJ[@]}" -o OmniWorkbench.exe \
    -lsqlite3 -lz -lws2_32 -lole32 -lshell32 -lcomdlg32 -luuid -lpthread < /dev/null
  cp -f "$WIN_STAGE_WSL/OmniWorkbench.exe" "$ROOT_DIR/build/win/OmniWorkbench.exe"
fi

ls -lh "$ROOT_DIR/build/win/OmniWorkbench.exe"
