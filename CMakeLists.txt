cmake_minimum_required(VERSION 3.10)

# Enable compilation database for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(LlaArbibot)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)

# Define the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

# Add the source files to the executable
file(GLOB SOURCES "src/*.cpp")
add_executable(LlaArbibot ${SOURCES})

# Find required packages
find_package(CURL REQUIRED)
find_package(jsoncpp CONFIG REQUIRED)
find_package(GTest REQUIRED)

# Add include directories for the source files
target_include_directories(LlaArbibot 
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JSONCPP_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

# Link with external libraries
target_link_libraries(LlaArbibot 
    PRIVATE
    CURL::libcurl 
    JsonCpp::JsonCpp
)

# Enable testing
enable_testing()

# Add subdirectories for tests
add_subdirectory(tests)
