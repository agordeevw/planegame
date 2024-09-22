cmake_minimum_required(VERSION 3.16.0)

file(GLOB_RECURSE PROJECT_SOURCES *.cpp **/*.cpp *.h **/*.h *.c **/*.c)

add_library(planegame.content SHARED ${PROJECT_SOURCES})

target_link_libraries(planegame.content
    SDL2::SDL2 glm::glm planegame.engine)

target_include_directories(planegame.content
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/content
        ${CMAKE_SOURCE_DIR}/src/engine)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES})

install(TARGETS planegame.content)