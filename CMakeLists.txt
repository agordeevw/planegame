cmake_minimum_required(VERSION 3.16.0)

project(planegame LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -D_DEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -D_DEBUG")

if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS)
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 ")
    set(CMAKE_SHARED_LINKER_FLAGS "-DEBUG")
    set(CMAKE_EXE_LINKER_FLAGS "-DEBUG")
    
    string(REGEX REPLACE "/W[0-9]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W3")
    
	set(MSVC_DISABLE_WARNINGS "4100;4251;4275" CACHE STRING "Disable specific warnings for MSVC")
    foreach(WARN_NUM ${MSVC_DISABLE_WARNINGS})
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd${WARN_NUM}")
    endforeach()
	
	add_compile_options("$<$<CONFIG:RELWITHDEBINFO>:/MDd>")
elseif(CMAKE_COMPILER_IS_GNUCXX)
	add_compile_options(-Wall)
	if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	    message("debug")
		add_compile_options(-g -DDEBUG)
	elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
	    message("release")
		add_compile_options(-O3 -DNDEBUG)
	else()
	    message("default")
		add_compile_options(-g -Os)
	endif()
else()
    message(FATAL_ERROR "Compiler not supported")
endif()

if(WIN32)
    set(CMAKE_BUILD_PARALLEL_LEVEL $ENV{NUMBER_OF_PROCESSORS})
endif()

add_subdirectory(src)