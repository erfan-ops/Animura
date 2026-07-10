cmake_minimum_required(VERSION 3.30)

project(Animura VERSION 1.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# Match the static CRT used by wallpaper_host (/MT, /MTd).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# ── Qt 6 (Core, Gui, Network, Widgets — QML/Quick replaced by WebView2 + React) ──
if(NOT CMAKE_PREFIX_PATH)
    if(DEFINED ENV{CMAKE_PREFIX_PATH} AND NOT "$ENV{CMAKE_PREFIX_PATH}" STREQUAL "")
        list(APPEND CMAKE_PREFIX_PATH $ENV{CMAKE_PREFIX_PATH})
    elseif(EXISTS "C:/Qt/6.8.2/msvc2022_64")
        list(APPEND CMAKE_PREFIX_PATH "C:/Qt/6.8.2/msvc2022_64")
    endif()
endif()

find_package(Qt6 6.8 REQUIRED COMPONENTS
    Core
    Gui
    Network
    Widgets
)

# ── vcpkg integration (for WebView2 SDK) ──
# Toolchain file should be set via CMakePresets.json "toolchainFile" field
# The static CRT triplet is listed first so that minizip-ng (which needs
# /MT to match the project) is resolved from the static build.
if(EXISTS "E:/coding/C/vcpkg/vcpkg/installed/x64-windows-static")
    list(APPEND CMAKE_PREFIX_PATH "E:/coding/C/vcpkg/vcpkg/installed/x64-windows-static")
endif()
if(EXISTS "E:/coding/C/vcpkg/vcpkg/installed/x64-windows")
    list(APPEND CMAKE_PREFIX_PATH "E:/coding/C/vcpkg/vcpkg/installed/x64-windows")
endif()

# ── WebView2 SDK ──
find_package(unofficial-webview2 CONFIG REQUIRED)

# ── minizip-ng (static CRT triplet required for /MT compatibility) ──
# Resolve zstd first so minizip-ng links libzstd_static, not libzstd_shared.
# Use NO_DEFAULT_PATH because CMAKE_PREFIX_PATH ordering alone doesn't
# guarantee the static triplet wins over the dynamic one during find_package.
set(_minizip_static "E:/coding/C/vcpkg/vcpkg/installed/x64-windows-static")
find_package(zstd CONFIG REQUIRED PATHS "${_minizip_static}" NO_DEFAULT_PATH)
find_package(minizip-ng CONFIG REQUIRED PATHS "${_minizip_static}" NO_DEFAULT_PATH)

# ── wallpaper_host static library ──
set(ANIMURA_LIB_DIR "${CMAKE_SOURCE_DIR}/lib" CACHE PATH
    "Directory containing wallpaper_host static libraries")

# ── Source files ──
set(ANIMURA_SOURCES
    src/main.cpp
    src/JsonUtils.cpp
    src/ModuleCatalog.cpp
    src/ModuleLibrary.cpp
    src/SettingsSchemaValidator.cpp
    src/WallpaperController.cpp
    src/NativeBridge.cpp
    src/WebView2Host.cpp
    src/ZipExtractor.cpp
)

# ── MOC headers (Qt meta-object compiler — Q_OBJECT classes) ──
# AUTOMOC handles these automatically; explicit listing ensures they are processed.
set(ANIMURA_MOC_HEADERS
    include/animura/WallpaperController.hpp
    include/animura/WebView2Host.hpp
)

# ── Resources (icon for system tray, Windows .rc for app icon) ──
set(ANIMURA_RESOURCES
    resources/resources.qrc
)

if(WIN32)
    list(APPEND ANIMURA_RESOURCES resources/Animura.rc)
endif()

qt_add_executable(Animura
    ${ANIMURA_SOURCES}
    ${ANIMURA_MOC_HEADERS}
    ${ANIMURA_RESOURCES}
)

target_include_directories(Animura PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
    "${CMAKE_SOURCE_DIR}/include/animura"
)

target_link_directories(Animura PRIVATE "${ANIMURA_LIB_DIR}")

target_link_libraries(Animura PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    Ole32
    $<$<CONFIG:Debug>:wallpaper_host_static_mtd>
    $<$<NOT:$<CONFIG:Debug>>:wallpaper_host_static_mt>
    MINIZIP::minizip-ng
)

# Link WebView2
if(unofficial-webview2_FOUND)
    target_link_libraries(Animura PRIVATE unofficial::webview2::webview2)
    message(STATUS "WebView2 SDK linked")
else()
    message(WARNING "WebView2 SDK not found via vcpkg — WebView2 features disabled")
endif()

# ── Compiler options ──
if(MSVC)
    target_compile_options(Animura PRIVATE /W3 /sdl /permissive- /MP)

    set_property(TARGET Animura APPEND PROPERTY LINK_OPTIONS
        "$<$<CONFIG:Debug>:/SUBSYSTEM:CONSOLE>"
        "$<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>"
        "$<$<CONFIG:RelWithDebInfo>:/SUBSYSTEM:WINDOWS>"
        "$<$<CONFIG:MinSizeRel>:/SUBSYSTEM:WINDOWS>"
        "$<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>"
    )

    set_property(TARGET Animura PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# ── Post-build: deploy Qt runtime (no --qmldir, no Quick imports — QML replaced by WebView2) ──
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")

if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET Animura POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --$<IF:$<CONFIG:Debug>,debug,release>
            --no-quick-import
            "$<TARGET_FILE:Animura>"
        COMMENT "Deploying Qt runtime with windeployqt (no QML)"
    )
endif()

# ── Post-build: copy wallpaper modules to build output ──
add_custom_command(TARGET Animura POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/modules
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/modules
    COMMENT "Copying wallpaper modules to build output"
)

# ── Post-build: copy React frontend dist to build output ──
if(EXISTS "${CMAKE_SOURCE_DIR}/frontend/dist")
    add_custom_command(TARGET Animura POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/frontend/dist"
            "$<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/frontend/dist"
        COMMENT "Copying React frontend to build output"
    )
endif()

# ── Convenience target: build the React frontend ──
find_program(NPM_EXECUTABLE npm)

if(NPM_EXECUTABLE AND EXISTS "${CMAKE_SOURCE_DIR}/frontend/package.json")
    add_custom_target(frontend
        COMMAND "${NPM_EXECUTABLE}" install
        COMMAND "${NPM_EXECUTABLE}" run build
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/frontend"
        COMMENT "Building React frontend with npm"
        BYPRODUCTS "${CMAKE_SOURCE_DIR}/frontend/dist"
    )
    message(STATUS "Frontend build target 'frontend' registered (cmake --build . --target frontend)")
endif()
