# Minimum CMake version required
cmake_minimum_required(VERSION 3.20)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel.")

# Project information
project(ConfQuery CXX)

include_directories(include)

# Set the required C++ standard.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS ON)

# Suppress iterator debug level warnings for MSVC in Debug mode
if(MSVC)
    add_compile_definitions($<$<CONFIG:Debug>:_ITERATOR_DEBUG_LEVEL=0>)
endif()

# Define the source files
set(SOURCE_FILES
    main.cpp
)

# Add executable target
add_executable(confq ${SOURCE_FILES})
