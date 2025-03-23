# Find GoogleTest package
find_package(GTest REQUIRED)

# Add test executable
add_executable(TimersTest 
    timers.test.cpp
    ../src/timers.cpp
)
add_executable(BalanceTest
    balance.test.cpp
    ../src/balance.cpp
)

# Link GoogleTest and pthread
target_link_libraries(TimersTest PRIVATE GTest::GTest GTest::Main pthread)
target_link_libraries(BalanceTest PRIVATE GTest::GTest GTest::Main pthread)

# Add include directory for source code headers
target_include_directories(TimersTest PRIVATE ../src)

# Register the test with CTest
add_test(NAME TimersTest COMMAND TimersTest)
add_test(NAME BalanceTest COMMAND BalanceTest)
set(CMAKE_TEST_VERBOSE 1)

# Suppress warnings for ignored attributes
# warning: 'maybe_unused' attribute for TEST, GTEST_TEST, etc
target_compile_options(TimersTest PRIVATE -Wno-ignored-attributes)
target_compile_options(BalanceTest PRIVATE -Wno-ignored-attributes)
