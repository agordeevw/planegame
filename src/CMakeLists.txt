cmake_minimum_required(VERSION 3.16.0)

set(PROJECT_NAME planegame)

set(SDL2_DIR "${CMAKE_SOURCE_DIR}/external/SDL2-2.26.1/cmake")
find_package(SDL2 REQUIRED)
set(SDL2_image_DIR "${CMAKE_SOURCE_DIR}/external/SDL2_image-2.6.2/cmake")
find_package(SDL2_image REQUIRED)

file(GLOB_RECURSE PROJECT_SOURCES *.cpp **/*.cpp *.h **/*.h *.c **/*.c)

add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

target_link_libraries(${PROJECT_NAME}
    SDL2::SDL2main SDL2::SDL2 glm::glm SDL2_image::SDL2_image)

target_include_directories(${PROJECT_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/planegame/Renderer
        ${CMAKE_SOURCE_DIR}/external/nlohmann/include)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCES})

install(TARGETS ${PROJECT_NAME})
install(IMPORTED_RUNTIME_ARTIFACTS SDL2::SDL2 SDL2_image::SDL2_image)