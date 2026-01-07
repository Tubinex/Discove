cmake_minimum_required(VERSION 3.15)

# Cross-Platform Build Instructions:
#
# Windows:
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows-static -DVCPKG_OVERLAY_PORTS=./vcpkg-overlays
#   cmake --build build --config Debug
#
# Linux:
#   vcpkg install fltk:x64-linux --overlay-ports="./vcpkg-overlays"
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_OVERLAY_PORTS=./vcpkg-overlays
#   cmake --build build
#
# macOS:
#   vcpkg install fltk:arm64-osx --overlay-ports="./vcpkg-overlays"
#   cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_OVERLAY_PORTS=./vcpkg-overlays
#   cmake --build build

if(POLICY CMP0091)
    cmake_policy(SET CMP0091 NEW)
    set(CMAKE_POLICY_DEFAULT_CMP0091 NEW CACHE STRING "" FORCE)
endif()

if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>" CACHE STRING "MSVC runtime library" FORCE)
endif()

project(Discove VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(FLTK CONFIG REQUIRED)
find_package(libwebsockets CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-sqlite3 CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

set(FONT_DIR "${CMAKE_SOURCE_DIR}/assets/fonts/inter")
set(FONT_HEADER_DIR "${CMAKE_BINARY_DIR}/generated/fonts")
file(MAKE_DIRECTORY "${FONT_HEADER_DIR}")

set(FONTS_AVAILABLE TRUE)
set(FONT_HEADERS "")

set(FONT_FILES
    "100.ttf:INTER_THIN_DATA"
    "200.ttf:INTER_EXTRA_LIGHT_DATA"
    "300.ttf:INTER_LIGHT_DATA"
    "400.ttf:INTER_REGULAR_DATA"
    "500.ttf:INTER_MEDIUM_DATA"
    "600.ttf:INTER_SEMIBOLD_DATA"
    "700.ttf:INTER_BOLD_DATA"
    "800.ttf:INTER_EXTRA_BOLD_DATA"
    "900.ttf:INTER_BLACK_DATA"
    "100italic.ttf:INTER_THIN_ITALIC_DATA"
    "200italic.ttf:INTER_EXTRA_LIGHT_ITALIC_DATA"
    "300italic.ttf:INTER_LIGHT_ITALIC_DATA"
    "400italic.ttf:INTER_REGULAR_ITALIC_DATA"
    "500italic.ttf:INTER_MEDIUM_ITALIC_DATA"
    "600italic.ttf:INTER_SEMIBOLD_ITALIC_DATA"
    "700italic.ttf:INTER_BOLD_ITALIC_DATA"
    "800italic.ttf:INTER_EXTRA_BOLD_ITALIC_DATA"
    "900italic.ttf:INTER_BLACK_ITALIC_DATA"
)

foreach(FONT_ENTRY ${FONT_FILES})
    string(REPLACE ":" ";" FONT_PARTS "${FONT_ENTRY}")
    list(GET FONT_PARTS 0 FONT_FILE)
    list(GET FONT_PARTS 1 FONT_VAR)

    set(FONT_PATH "${FONT_DIR}/${FONT_FILE}")
    string(REPLACE ".ttf" "" FONT_NAME_CLEAN "${FONT_FILE}")
    string(REPLACE "-" "_" FONT_NAME_CLEAN "${FONT_NAME_CLEAN}")
    set(HEADER_PATH "${FONT_HEADER_DIR}/${FONT_NAME_CLEAN}.h")

    if(EXISTS "${FONT_PATH}")
        message(STATUS "Found font: ${FONT_FILE}, generating header...")
        execute_process(
            COMMAND python ${CMAKE_SOURCE_DIR}/cmake/generate_font_header.py ${FONT_PATH} ${HEADER_PATH} ${FONT_VAR}
            RESULT_VARIABLE FONT_RESULT
            OUTPUT_VARIABLE FONT_OUTPUT
            ERROR_VARIABLE FONT_ERROR
        )

        if(FONT_RESULT EQUAL 0)
            list(APPEND FONT_HEADERS "${HEADER_PATH}")
            message(STATUS "${FONT_OUTPUT}")
        else()
            message(WARNING "Failed to generate header for ${FONT_FILE}")
            if(FONT_ERROR)
                message(WARNING "${FONT_ERROR}")
            endif()
            set(FONTS_AVAILABLE FALSE)
        endif()
    else()
        message(WARNING "Font file not found: ${FONT_PATH}")
        message(STATUS "Fonts will be loaded from files at runtime (if available)")
        set(FONTS_AVAILABLE FALSE)
    endif()
endforeach()

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS} ${FONT_HEADERS})

if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/generated
)

if(FONTS_AVAILABLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FONTS_EMBEDDED)
    message(STATUS "Fonts will be embedded in the executable")
else()
    message(STATUS "Fonts will be loaded from assets/fonts/ at runtime")
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    fltk fltk_images
    websockets
    CURL::libcurl
    nlohmann_json::nlohmann_json
    unofficial::sqlite3::sqlite3
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 comctl32 bcrypt Crypt32 gdiplus gdi32 dbghelp)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    endif()
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(SECURITY_LIBRARY Security)
    find_library(CORETEXT_LIBRARY CoreText)
    find_library(COREFOUNDATION_LIBRARY CoreFoundation)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        ${COCOA_LIBRARY}
        ${SECURITY_LIBRARY}
        ${CORETEXT_LIBRARY}
        ${COREFOUNDATION_LIBRARY}
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GIFLIB REQUIRED giflib)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GIFLIB_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GIFLIB_INCLUDE_DIRS})
elseif(UNIX)
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES} pthread)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(LIBSECRET REQUIRED libsecret-1)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${LIBSECRET_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${LIBSECRET_INCLUDE_DIRS})

    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${FONTCONFIG_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${FONTCONFIG_INCLUDE_DIRS})

    pkg_check_modules(WAYLAND wayland-client)
    if(WAYLAND_FOUND)
        target_link_libraries(${PROJECT_NAME} PRIVATE ${WAYLAND_LIBRARIES})
        target_include_directories(${PROJECT_NAME} PRIVATE ${WAYLAND_INCLUDE_DIRS})
    endif()

    pkg_check_modules(GIFLIB REQUIRED giflib)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${GIFLIB_LIBRARIES})
    target_include_directories(${PROJECT_NAME} PRIVATE ${GIFLIB_INCLUDE_DIRS})

    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads dl)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION bin)
