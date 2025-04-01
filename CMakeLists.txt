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
find_package(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)

# Include FetchContent module
include(FetchContent)

# Fetch nlohmann/json
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.2.tar.gz
)
FetchContent_MakeAvailable(json)

# Add source files
set(SOURCES
    src/event_loop.cpp
    src/timers.cpp
    src/api_binance.cpp
    src/api_kraken.cpp
    src/s_poplavki.cpp
    src/orderbook.cpp
    src/balance.cpp
    src/tracer.cpp
    src/ex_mgr.cpp
    src/api_exchange.cpp
)

# Set header files
set(HEADERS
    src/event_loop.h
    src/timers.h
    src/api_exchange.h
    src/api_binance.h
    src/api_kraken.h
    src/s_poplavki.h
    src/orderbook.h
    src/balance.h
    src/tracer.h
    src/types.h
)

# Create library from source files
add_library(lla_arbibot_lib STATIC ${SOURCES} ${HEADERS})

# Add include directories
target_include_directories(lla_arbibot_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    ${nlohmann_json_SOURCE_DIR}/include
)

# Link libraries
target_link_libraries(lla_arbibot_lib PUBLIC
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    pthread
    nlohmann_json::nlohmann_json
)

# Create main executable
add_executable(LlaArbibot src/main.cpp)
target_link_libraries(LlaArbibot PRIVATE lla_arbibot_lib)

# Add test executables
add_executable(api_test 
    tests/api.test.cpp 
    tests/mock_api.cpp
)
target_link_libraries(api_test PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(BalanceTest 
    tests/balance.test.cpp
)
target_link_libraries(BalanceTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(OrderBookTest 
    tests/orderbook.test.cpp
)
target_link_libraries(OrderBookTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(TimersTest 
    tests/timers.test.cpp
)
target_link_libraries(TimersTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(EventLoopTest 
    tests/event_loop.test.cpp
)
target_link_libraries(EventLoopTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

# Add tests to CTest
add_test(NAME ApiTest COMMAND api_test)
add_test(NAME BalanceTest COMMAND BalanceTest)
add_test(NAME OrderBookTest COMMAND OrderBookTest)
add_test(NAME TimersTest COMMAND TimersTest)
add_test(NAME EventLoopTest COMMAND EventLoopTest)
