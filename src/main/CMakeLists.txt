cmake_minimum_required(VERSION 3.16.0)

file(GLOB_RECURSE PROJECT_SOURCES *.cpp **/*.cpp *.h **/*.h *.c **/*.c)

add_executable(planegame.main ${PROJECT_SOURCES})

set_property(TARGET planegame.main PROPERTY VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

target_link_libraries(planegame.main
    SDL2::SDL2main planegame.content planegame.engine)

target_include_directories(planegame.main
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/engine
        ${CMAKE_SOURCE_DIR}/src/content)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES})

install(TARGETS planegame.main)