cmake_minimum_required(VERSION 3.15)
project(Discove VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

find_package(FLTK REQUIRED)
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${FLTK_INCLUDE_DIR}
)

file(GLOB_RECURSE SOURCES
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS
    "${CMAKE_SOURCE_DIR}/include/*.h"
    "${CMAKE_SOURCE_DIR}/include/*.hpp"
)

add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})
target_link_libraries(${PROJECT_NAME} PRIVATE ${FLTK_LIBRARIES})

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32 comctl32)
    if(MSVC)
        target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
    endif()
elseif(APPLE)
    find_library(COCOA_LIBRARY Cocoa)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${COCOA_LIBRARY})
elseif(UNIX)
    find_package(X11 REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE ${X11_LIBRARIES} pthread)

    find_package(PkgConfig)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(WAYLAND wayland-client)
        if(WAYLAND_FOUND)
            target_link_libraries(${PROJECT_NAME} PRIVATE ${WAYLAND_LIBRARIES})
            target_include_directories(${PROJECT_NAME} PRIVATE ${WAYLAND_INCLUDE_DIRS})
        endif()
    endif()

    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads dl)
endif()

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION bin)
