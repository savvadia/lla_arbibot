# Find GoogleTest package
find_package(GTest REQUIRED)

# Add test executables
add_executable(TimersTest 
    timers.test.cpp
    timers_perf.test.cpp
    ../src/timers.cpp
    ../src/event_loop.cpp
    ../src/tracer.cpp
)
add_executable(BalanceTest
    balance.test.cpp
    ../src/balance.cpp
    ../src/tracer.cpp
)
add_executable(EventLoopTest
    event_loop.test.cpp
    ../src/event_loop.cpp
    ../src/tracer.cpp
)
add_executable(ApiTest
    api.test.cpp
    mock_api.cpp
    ${CMAKE_SOURCE_DIR}/src/api_exchange.cpp
    ${CMAKE_SOURCE_DIR}/src/api_binance.cpp
    ${CMAKE_SOURCE_DIR}/src/api_kraken.cpp
    ${CMAKE_SOURCE_DIR}/src/orderbook.cpp
    ${CMAKE_SOURCE_DIR}/src/event_loop.cpp
    ${CMAKE_SOURCE_DIR}/src/timers.cpp
    ${CMAKE_SOURCE_DIR}/src/balance.cpp
    ${CMAKE_SOURCE_DIR}/src/tracer.cpp
)
add_executable(OrderBookTest
    orderbook.test.cpp
    ${CMAKE_SOURCE_DIR}/src/orderbook.cpp
    ${CMAKE_SOURCE_DIR}/src/tracer.cpp
)

# Link GoogleTest and pthread
target_link_libraries(TimersTest PRIVATE GTest::GTest GTest::Main pthread)
target_link_libraries(BalanceTest PRIVATE GTest::GTest GTest::Main pthread)
target_link_libraries(EventLoopTest PRIVATE GTest::GTest GTest::Main pthread)
target_link_libraries(ApiTest PRIVATE 
    GTest::gtest
    GTest::gtest_main
    pthread
    ${Boost_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    ${CURL_LIBRARIES}
)
target_link_libraries(OrderBookTest PRIVATE GTest::GTest GTest::Main pthread)

# Add include directory for source code headers
target_include_directories(TimersTest PRIVATE ../src)
target_include_directories(BalanceTest PRIVATE ../src)
target_include_directories(EventLoopTest PRIVATE ../src)
target_include_directories(ApiTest PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${Boost_INCLUDE_DIRS}
)
target_include_directories(OrderBookTest PRIVATE ../src)

# Register the test with CTest
add_test(NAME TimersTest COMMAND TimersTest)
add_test(NAME BalanceTest COMMAND BalanceTest)
add_test(NAME EventLoopTest COMMAND EventLoopTest)
add_test(NAME ApiTest COMMAND ApiTest)
add_test(NAME OrderBookTest COMMAND OrderBookTest)
set(CMAKE_TEST_VERBOSE 1)

# Suppress warnings for ignored attributes
# warning: 'maybe_unused' attribute for TEST, GTEST_TEST, etc
target_compile_options(TimersTest PRIVATE -Wno-ignored-attributes)
target_compile_options(BalanceTest PRIVATE -Wno-ignored-attributes)
target_compile_options(EventLoopTest PRIVATE -Wno-ignored-attributes)
target_compile_options(ApiTest PRIVATE -Wno-ignored-attributes)
target_compile_options(OrderBookTest PRIVATE -Wno-ignored-attributes)
