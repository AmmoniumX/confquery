cmake_minimum_required(VERSION 3.30)

# Use exeperimental gate UUID for CMake v4.2.3
set(CMAKE_EXPERIMENTAL_CXX_IMPORT_STD "d0edc3af-4c50-42ea-a356-e2862fe7a444")

# Set toolchain hints before project()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Must be set before project() so compiler detection uses libc++ modules
set(CMAKE_CXX_FLAGS "-stdlib=libc++" CACHE STRING "" FORCE)

# Initialize project
project(ConfQuery CXX)

# Target configuration
add_executable(confq main.cpp)

# Enable module support for this target
set_target_properties(confq PROPERTIES 
    CXX_SCAN_FOR_MODULES ON
    CXX_MODULE_STD ON
)

