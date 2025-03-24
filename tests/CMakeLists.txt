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

# Link GoogleTest and pthread
target_link_libraries(TimersTest PRIVATE GTest::GTest GTest::Main pthread)
target_link_libraries(BalanceTest PRIVATE GTest::GTest GTest::Main pthread)
target_link_libraries(EventLoopTest PRIVATE GTest::GTest GTest::Main pthread)

# Add include directory for source code headers
target_include_directories(TimersTest PRIVATE ../src)
target_include_directories(BalanceTest PRIVATE ../src)
target_include_directories(EventLoopTest PRIVATE ../src)

# Register the test with CTest
add_test(NAME TimersTest COMMAND TimersTest)
add_test(NAME BalanceTest COMMAND BalanceTest)
add_test(NAME EventLoopTest COMMAND EventLoopTest)
set(CMAKE_TEST_VERBOSE 1)

# Suppress warnings for ignored attributes
# warning: 'maybe_unused' attribute for TEST, GTEST_TEST, etc
target_compile_options(TimersTest PRIVATE -Wno-ignored-attributes)
target_compile_options(BalanceTest PRIVATE -Wno-ignored-attributes)
target_compile_options(EventLoopTest PRIVATE -Wno-ignored-attributes)
