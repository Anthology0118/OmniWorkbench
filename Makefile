CXX_LINUX := g++
CXX_WIN   := /mnt/d/msys/ucrt64/bin/g++.exe

CXXFLAGS_COMMON := -std=c++17 -O2 -Wall -Wextra -Isrc
WIN_INCLUDE     := -I/mnt/d/msys/ucrt64/include
WIN_LIB         := -L/mnt/d/msys/ucrt64/lib

CORE_SRCS := \
	src/core/config.cpp \
	src/core/dict_manager.cpp \
	src/core/docx_converter.cpp \
	src/core/json.cpp \
	src/core/markdown.cpp \
	src/core/pdf_converter.cpp \
	src/core/platform_utils.cpp \
	src/core/vault_manager.cpp \
	src/core/xml_doc.cpp \
	src/core/zip_archive.cpp \
	src/server/app_server.cpp

UI_HTML := web/index.html
UI_HPP  := src/generated/embedded_ui.hpp

.PHONY: all linux windows test clean

all: linux test

$(UI_HPP): $(UI_HTML) scripts/embed_ui.py
	python3 scripts/embed_ui.py $(UI_HTML) $(UI_HPP)

build/linux/omniworkbench: $(UI_HPP) $(CORE_SRCS) src/desktop/main.cpp
	@mkdir -p build/linux
	$(CXX_LINUX) $(CXXFLAGS_COMMON) $(CORE_SRCS) src/desktop/main.cpp -o $@ -lz -lpthread

build/linux/omni_tests: $(UI_HPP) $(CORE_SRCS) tests/test_main.cpp
	@mkdir -p build/linux
	$(CXX_LINUX) $(CXXFLAGS_COMMON) $(CORE_SRCS) tests/test_main.cpp -o $@ -lz -lpthread

linux: build/linux/omniworkbench

windows: $(UI_HPP)
	bash scripts/build_windows.sh

test: build/linux/omni_tests
	./build/linux/omni_tests

clean:
	rm -rf build $(UI_HPP)
