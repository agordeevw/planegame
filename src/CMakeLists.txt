cmake_minimum_required(VERSION 3.16.0)

set(SDL2_DIR "${CMAKE_SOURCE_DIR}/external/SDL2-2.26.1/cmake")
find_package(SDL2 REQUIRED)
set(SDL2_image_DIR "${CMAKE_SOURCE_DIR}/external/SDL2_image-2.6.2/cmake")
find_package(SDL2_image REQUIRED)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_subdirectory(engine)
add_subdirectory(content)
add_subdirectory(main)