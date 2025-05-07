# set limits due to nlohmann/json needs to be updated
cmake_minimum_required(VERSION 3.31)
set(CMAKE_OSX_DEPLOYMENT_TARGET "15.0")

# Set CMake policies for newer CMake version
cmake_policy(SET CMP0167 NEW) # Use the new FindBoost module
cmake_policy(SET CMP0135 NEW) # Use the new FetchContent behavior
cmake_policy(SET CMP0091 NEW) # Enable new MSVC runtime library behavior

# Enable compilation database for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set the SDK version explicitly
set(CMAKE_OSX_SYSROOT "/Library/Developer/CommandLineTools/SDKs/MacOSX15.4.sdk" CACHE PATH "SDK path")

# Set the deployment target
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum OS X deployment version")

# Set curl paths explicitly
set(CURL_ROOT "/opt/homebrew/opt/curl")
set(CURL_INCLUDE_DIR "${CURL_ROOT}/include")
set(CURL_LIBRARY "${CURL_ROOT}/lib/libcurl.dylib")

project(LlaArbibot)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Define the compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -O2")

# Set output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Find required packages
find_package(CURL REQUIRED)
find_package(Boost REQUIRED COMPONENTS system thread)
find_package(GTest REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(ZLIB REQUIRED)

# Include FetchContent module
include(FetchContent)

# Fetch nlohmann/json with specific version and options
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/archive/refs/tags/v3.11.3.tar.gz
)
FetchContent_MakeAvailable(json)

# Add source files
file(GLOB_RECURSE SOURCES "src/*.cpp")
file(GLOB_RECURSE HEADERS "src/*.h")

# Create library from source files
add_library(lla_arbibot_lib STATIC ${SOURCES} ${HEADERS})

# Add include directories
target_include_directories(lla_arbibot_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${Boost_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
    nlohmann_json::nlohmann_json
)

# Link libraries
target_link_libraries(lla_arbibot_lib PUBLIC
    ${Boost_LIBRARIES}
    ${CURL_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    pthread
    nlohmann_json::nlohmann_json
)

# Create main executable
add_executable(LlaArbibot src/main.cpp)
target_link_libraries(LlaArbibot PRIVATE lla_arbibot_lib)

# Add test executables
add_executable(BalanceTest tests/balance.test.cpp)
target_link_libraries(BalanceTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(OrderBookTest tests/orderbook.test.cpp)
target_link_libraries(OrderBookTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(TimersTest tests/timers.test.cpp tests/timers_perf.test.cpp)
target_link_libraries(TimersTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(EventLoopTest tests/event_loop.test.cpp)
target_link_libraries(EventLoopTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(ApiExchangeTest tests/api_exchange.test.cpp)
target_link_libraries(ApiExchangeTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(ApiBinanceTest tests/api_binance.test.cpp)
target_link_libraries(ApiBinanceTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)

add_executable(ApiKrakenTest tests/api_kraken.test.cpp)
target_link_libraries(ApiKrakenTest PRIVATE lla_arbibot_lib GTest::GTest GTest::Main)


# Register tests with CTest
add_test(NAME BalanceTest COMMAND BalanceTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME OrderBookTest COMMAND OrderBookTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME TimersTest COMMAND TimersTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME EventLoopTest COMMAND EventLoopTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME ApiExchangeTest COMMAND ApiExchangeTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME ApiBinanceTest COMMAND ApiBinanceTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
add_test(NAME ApiKrakenTest COMMAND ApiKrakenTest WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Suppress warnings for ignored attributes
target_compile_options(BalanceTest PRIVATE -Wno-ignored-attributes)
target_compile_options(OrderBookTest PRIVATE -Wno-ignored-attributes)
target_compile_options(TimersTest PRIVATE -Wno-ignored-attributes)
target_compile_options(EventLoopTest PRIVATE -Wno-ignored-attributes)
target_compile_options(ApiExchangeTest PRIVATE -Wno-ignored-attributes)
target_compile_options(ApiBinanceTest PRIVATE -Wno-ignored-attributes)
target_compile_options(ApiKrakenTest PRIVATE -Wno-ignored-attributes)

# Remove the tests subdirectory since all test content is here
# add_subdirectory(tests)
