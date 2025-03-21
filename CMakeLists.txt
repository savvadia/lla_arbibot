cmake_minimum_required(VERSION 3.10)

project(LlaArbibot)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Define the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

# Add include directories for the source files
include_directories(src)

# Add the source files to the executable
add_executable(LlaArbibot src/main.cpp src/timers.cpp)

# Link with external libraries (curl, jsoncpp)
find_package(CURL REQUIRED)
find_package(jsoncpp REQUIRED)

target_link_libraries(LlaArbibot CURL::libcurl jsoncpp)

# Add include directories for the source files (make sure to add the src/ folder here)
target_include_directories(LlaArbibot PRIVATE src)

# Enable testing
enable_testing()

# Add subdirectories for tests
add_subdirectory(tests)

# Link with GoogleTest for unit tests
find_package(GTest REQUIRED)
target_include_directories(LlaArbibot PRIVATE ${GTEST_INCLUDE_DIRS})
