cmake_minimum_required(VERSION 3.16.0)

set(PROJECT_NAME planegame)

set(SDL2_DIR "${CMAKE_SOURCE_DIR}/external/SDL2-2.26.1/cmake")
find_package(SDL2 REQUIRED)

file(GLOB_RECURSE PROJECT_SOURCES
    glad/glad.h
    glad/glad.c
    Main.cpp)

add_executable(${PROJECT_NAME} WIN32 ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME}
    SDL2::SDL2main SDL2::SDL2)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${SDL2_INCLUDE_DIR})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES})

install(TARGETS ${PROJECT_NAME})
install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2)