cmake_minimum_required(VERSION 3.16.0)

file(GLOB_RECURSE PROJECT_SOURCES *.cpp **/*.cpp *.h **/*.h *.c **/*.c)

add_library(planegame.engine SHARED ${PROJECT_SOURCES})

target_link_libraries(planegame.engine
    SDL2::SDL2 glm::glm SDL2_image::SDL2_image)

target_include_directories(planegame.engine
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src/engine
        ${CMAKE_SOURCE_DIR}/src/engine/engine/Renderer
        ${CMAKE_SOURCE_DIR}/external/nlohmann/include)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES})

install(TARGETS planegame.engine)
install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2 SDL2_image::SDL2_image)