cmake_minimum_required(VERSION 3.25)

project(FLFHMCLauncher VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(MSVC)
    # 源码为 UTF-8；中文 Windows 默认 GBK 代码页会把注释/字符串解析坏
    add_compile_options(/utf-8 /permissive-)
endif()

option(FLF_ENABLE_HIBERGUI "Build and link HiBerGUILibrary (needs nlohmann-json)" ON)
option(FLF_ENABLE_WEBVIEW2 "Build HiBerGUIWebView2 (needs vcpkg webview2)" OFF)
option(FLF_BUILD_QUICKSTART "Build the tiny quick-start launcher" ON)
option(FLF_BUILD_EXAMPLES "Build examples" OFF)

# ---------------------------------------------------------------------------
# 依赖
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(nlohmann_json CONFIG REQUIRED)

qt_standard_project_setup()

set(COMMONLOGGER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/CommonLoggerCPP EXCLUDE_FROM_ALL)

set_target_properties(CommonLoggerCPP PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_subdirectory(third_party/CrashTrackerHandleLibCPP EXCLUDE_FROM_ALL)

if(FLF_ENABLE_HIBERGUI)
    add_subdirectory(
        third_party/HiBerGUILibCPP/HiBerGUILibrary
        ${CMAKE_BINARY_DIR}/third_party/HiBerGUILibrary
    )
endif()

if(FLF_ENABLE_WEBVIEW2)
    find_package(unofficial-webview2 CONFIG REQUIRED)
    add_subdirectory(
        third_party/HiBerGUILibCPP/HiBerGUIWebView2
        ${CMAKE_BINARY_DIR}/third_party/HiBerGUIWebView2
    )
endif()

# ---------------------------------------------------------------------------
# Core
# ---------------------------------------------------------------------------
add_library(FLFLauncherCore STATIC
    src/core/flf_paths.cpp
    src/core/app_bootstrap.cpp
    src/core/workspace.cpp
    src/core/http.cpp
    src/core/meta.cpp
    src/core/installer.cpp
    src/core/library_table.cpp
    src/core/launcher.cpp
    src/core/mods.cpp
    src/core/quickstart_config.cpp
)

target_include_directories(FLFLauncherCore PUBLIC src)

target_link_libraries(FLFLauncherCore
    PUBLIC
        CommonLoggerCPP
        CrashTrackerHandleLib
        nlohmann_json::nlohmann_json
        Qt6::Core
        winhttp
)

if(FLF_ENABLE_HIBERGUI)
    target_link_libraries(FLFLauncherCore PUBLIC HiBerGUILibrary)
    target_compile_definitions(FLFLauncherCore PUBLIC FLF_HAVE_HIBERGUI=1)
endif()

# ---------------------------------------------------------------------------
# GUI launcher
# ---------------------------------------------------------------------------
qt_add_executable(FLFHMCLauncher WIN32
    src/main.cpp
    src/ui/mainwindow.cpp
    src/ui/mainwindow.h
)

qt_add_resources(FLFHMCLauncher "app_resources"
    PREFIX "/"
    FILES assets/icon/flfhmlauncher-256.png
)

if(MSVC)
    target_sources(FLFHMCLauncher PRIVATE resources/flfhmlauncher.rc)
endif()

target_link_libraries(FLFHMCLauncher PRIVATE FLFLauncherCore Qt6::Widgets)

if(TARGET Qt6::QWindowsIntegrationPlugin)
    qt_import_plugins(FLFHMCLauncher INCLUDE Qt::QWindowsIntegrationPlugin)
endif()

set_target_properties(FLFHMCLauncher PROPERTIES
    OUTPUT_NAME "FLFHMCLauncher"
    WIN32_EXECUTABLE TRUE
)

# ---------------------------------------------------------------------------
# CLI（自测 / 无界面安装）
# ---------------------------------------------------------------------------
add_executable(FLFLauncherCli src/cli_main.cpp)
target_link_libraries(FLFLauncherCli PRIVATE FLFLauncherCore Qt6::Core)

# ---------------------------------------------------------------------------
# Quick start EXE (no Qt)
# ---------------------------------------------------------------------------
if(FLF_BUILD_QUICKSTART)
    add_executable(FLFQuickStart WIN32
        src/quickstart_main.cpp
        src/core/quickstart_config.cpp
    )
    target_include_directories(FLFQuickStart PRIVATE src)
    target_link_libraries(FLFQuickStart PRIVATE shell32)
    set_target_properties(FLFQuickStart PROPERTIES OUTPUT_NAME "FLFQuickStart")
endif()

install(TARGETS FLFHMCLauncher RUNTIME DESTINATION .)
if(FLF_BUILD_QUICKSTART)
    install(TARGETS FLFQuickStart RUNTIME DESTINATION .)
endif()
