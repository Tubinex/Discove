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
find_package(ixwebsocket CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

if(MSVC)
    set_property(TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(${PROJECT_NAME} PRIVATE
    fltk fltk_images
    ixwebsocket::ixwebsocket
    nlohmann_json::nlohmann_json
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 comctl32 bcrypt Crypt32 gdiplus)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    endif()
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    find_library(SECURITY_LIBRARY Security)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY} ${SECURITY_LIBRARY})

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
