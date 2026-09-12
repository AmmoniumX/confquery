# Minimum CMake version required
cmake_minimum_required(VERSION 3.20)

set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel.")

# Project information
project(ConfQuery CXX)

include_directories(include)

# Set the required C++ standard.
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add doctest
add_subdirectory(doctest)

# Library containing the parser/query logic (confquery.hpp/.cpp), shared by
# the CLI executable and the unit tests.
add_library(confquery_lib STATIC confquery.cpp)
target_include_directories(confquery_lib PUBLIC ${CMAKE_SOURCE_DIR})

# Add executable target
add_executable(confq main.cpp)
target_link_libraries(confq PRIVATE confquery_lib)
target_include_directories(confq PRIVATE ${CMAKE_SOURCE_DIR}/ArgDispatch/include)

# Unit tests
enable_testing()
add_executable(confquery_tests tests/test_confquery.cpp)
target_link_libraries(confquery_tests PRIVATE confquery_lib doctest::doctest_with_main)
add_test(NAME confquery_tests COMMAND confquery_tests)
