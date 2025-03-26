cmake_minimum_required(VERSION 3.10)

# Enable compilation database for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(LlaArbibot)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

# Find required packages
find_package(OpenSSL REQUIRED)
find_package(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(Threads REQUIRED)

# Include FetchContent module
include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz
)
FetchContent_MakeAvailable(json)

# Set source files
set(SOURCES
    src/main.cpp
    src/event_loop.cpp
    src/tracer.cpp
    src/balance.cpp
    src/timers.cpp
    src/api_binance.cpp
    src/api_kraken.cpp
    src/s_poplavki.cpp
    src/orderbook.cpp
    src/ex_mgr.cpp
    src/exchange_api.cpp
)

set(HEADERS
    src/balance.h
    src/event_loop.h
    src/tracer.h
    src/timers.h
    src/api_binance.h
    src/api_kraken.h
    src/s_poplavki.h
    src/orderbook.h
    src/exchange_api.h
    src/ex_mgr.h
)

# Add executable
add_executable(LlaArbibot ${SOURCES} ${HEADERS})

# Add include directories
target_include_directories(LlaArbibot PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
    ${nlohmann_json_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(LlaArbibot PRIVATE
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${Boost_LIBRARIES}
    Threads::Threads
    nlohmann_json::nlohmann_json
)

# Enable testing
enable_testing()

# Add subdirectories for tests
add_subdirectory(tests)
