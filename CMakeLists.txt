cmake_minimum_required(VERSION 3.21)

project(Animura VERSION 1.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Match the static CRT used by wallpaper_host (/MT, /MTd).
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(ANIMURA_LIB_DIR "${CMAKE_SOURCE_DIR}/lib" CACHE PATH
    "Directory containing wallpaper_host static libraries")

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
    Qml
    Quick
    QuickControls2
    QuickDialogs2
    QuickLayouts
    Widgets
    QuickWidgets
)

set(ANIMURA_SOURCES
    src/main.cpp
    src/JsonUtils.cpp
    src/ModuleCatalog.cpp
    src/ModuleLibrary.cpp
    src/SettingsSchemaValidator.cpp
    src/WallpaperController.cpp
)

set(ANIMURA_MOC_HEADERS
    include/animura/ColorDialogHelper.hpp
    include/animura/WallpaperController.hpp
)

set(ANIMURA_RESOURCES
    resources/qml.qrc
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
    Qt6::Qml
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::QuickLayouts
    Qt6::Widgets
    Qt6::QuickWidgets
    Ole32
    Dwmapi
    $<$<CONFIG:Debug>:wallpaper_host_static_mtd>
    $<$<NOT:$<CONFIG:Debug>>:wallpaper_host_static_mt>
)

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

find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${Qt6_DIR}/../../../bin")

if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET Animura POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            --$<IF:$<CONFIG:Debug>,debug,release>
            --qmldir "${CMAKE_SOURCE_DIR}/qml"
            "$<TARGET_FILE:Animura>"
        COMMENT "Deploying Qt runtime with windeployqt"
    )
endif()

add_custom_command(TARGET Animura POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/modules
        $<TARGET_FILE_DIR:${CMAKE_PROJECT_NAME}>/modules
)