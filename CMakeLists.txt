cmake_minimum_required(VERSION 3.16)
project(OmniWorkbench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ENABLE_QT6 "Build native Qt 6 WebEngine desktop shell when Qt6 is installed" OFF)

find_package(ZLIB REQUIRED)
find_package(Threads REQUIRED)
find_package(SQLite3 QUIET)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(GENERATED_UI_HPP "${CMAKE_CURRENT_SOURCE_DIR}/src/generated/embedded_ui.hpp")
add_custom_command(
    OUTPUT "${GENERATED_UI_HPP}"
    COMMAND ${Python3_EXECUTABLE} "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_ui.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html" "${GENERATED_UI_HPP}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/index.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_ui.py"
    COMMENT "Embedding web/index.html into C++ header"
)
add_custom_target(generate_ui DEPENDS "${GENERATED_UI_HPP}")

set(OMNI_CORE_SOURCES
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

add_library(omni_core STATIC ${OMNI_CORE_SOURCES})
add_dependencies(omni_core generate_ui)
target_include_directories(omni_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(omni_core PUBLIC ZLIB::ZLIB Threads::Threads)

if(SQLite3_FOUND)
    target_compile_definitions(omni_core PUBLIC OMNI_USE_SQLITE3=1)
    target_link_libraries(omni_core PUBLIC SQLite::SQLite3)
endif()

if(WIN32)
    target_link_libraries(omni_core PUBLIC ws2_32 ole32 shell32 comdlg32 uuid)
endif()

add_executable(OmniWorkbench src/desktop/main.cpp)
target_link_libraries(OmniWorkbench PRIVATE omni_core)
if(WIN32)
    set_target_properties(OmniWorkbench PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

add_executable(omni_tests tests/test_main.cpp)
target_link_libraries(omni_tests PRIVATE omni_core)

if(ENABLE_QT6)
    find_package(Qt6 REQUIRED COMPONENTS Widgets)
    add_executable(OmniWorkbenchQt src/desktop/qt_main.cpp)
    target_link_libraries(OmniWorkbenchQt PRIVATE omni_core Qt6::Widgets)
    if(WIN32)
        set_target_properties(OmniWorkbenchQt PROPERTIES WIN32_EXECUTABLE TRUE)
    endif()
endif()
