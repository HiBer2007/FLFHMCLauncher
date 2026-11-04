cmake_minimum_required(VERSION 3.25)

project(FLFHMCLauncher VERSION 0.1.0 LANGUAGES CXX)

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
# Qt (static build expected)
# ---------------------------------------------------------------------------
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

# ---------------------------------------------------------------------------
# third_party submodules
# ---------------------------------------------------------------------------
set(COMMONLOGGER_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/CommonLoggerCPP EXCLUDE_FROM_ALL)

# 子模块自己把 CXX_STANDARD 设成 17；spdlog 的 SPDLOG_USE_STD_FORMAT 需要 C++20
set_target_properties(CommonLoggerCPP PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
)

add_subdirectory(third_party/CrashTrackerHandleLibCPP EXCLUDE_FROM_ALL)

if(FLF_ENABLE_HIBERGUI)
    find_package(nlohmann_json CONFIG REQUIRED)
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
    src/core/quickstart_config.cpp
)

target_include_directories(FLFLauncherCore PUBLIC src)

target_link_libraries(FLFLauncherCore
    PUBLIC
        CommonLoggerCPP
        CrashTrackerHandleLib
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
)

target_link_libraries(FLFHMCLauncher PRIVATE FLFLauncherCore Qt6::Widgets)

# 静态 Qt 需要显式导入平台插件
if(TARGET Qt6::QWindowsIntegrationPlugin)
    qt_import_plugins(FLFHMCLauncher INCLUDE Qt::QWindowsIntegrationPlugin)
endif()

set_target_properties(FLFHMCLauncher PROPERTIES
    OUTPUT_NAME "FLFHMCLauncher"
    WIN32_EXECUTABLE TRUE
)

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

# ---------------------------------------------------------------------------
# Install layout: EXE + .flfhmcl is created at first run
# ---------------------------------------------------------------------------
install(TARGETS FLFHMCLauncher RUNTIME DESTINATION .)
if(FLF_BUILD_QUICKSTART)
    install(TARGETS FLFQuickStart RUNTIME DESTINATION .)
endif()
