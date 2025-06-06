#pragma once

namespace Config {
    // Execution settings
    constexpr int EVENT_LOOP_DELAY_MS = 10;  // Delay between event loop iterations
    constexpr int MAX_EXECUTION_TIME_MS = 0;  // Maximum time to run the bot (0 = run indefinitely)
    constexpr int MAX_CALLBACK_EXECUTION_TIME_MS = 10;
    constexpr int MAX_TIMER_DELAY_TRACE_MS = EVENT_LOOP_DELAY_MS * 2;

    // Strategy settings
    constexpr int STRATEGY_CHECK_TIMER_MS = 5000;  // How often to check for opportunities
    constexpr int BEST_SEEN_OPPORTUNITY_RESET_INTERVAL_MS = 600000; // 10 minutes
    constexpr double MIN_EXECUTION_MARGIN = 0.02;  // Minimum margin to execute a trade, %
    constexpr double MIN_TRACEABLE_MARGIN = 0.01;  // Minimum margin to trace, %
    constexpr int OPPORTUNITY_TIMEOUT_MS = 2000; // 10 seconds

    // Order settings
    constexpr int ORDER_TEST_STATE_CHANGE_DELAY_MS = OPPORTUNITY_TIMEOUT_MS / 2;

    // Traces settings
    constexpr int COUNTABLE_TRACES_PRINT_INTERVAL1 = 10;
    constexpr int COUNTABLE_TRACES_PRINT_INTERVAL2 = 100;
    constexpr int COUNTABLE_TRACES_PRINT_INTERVAL3 = 1000;
    constexpr int COUNTABLE_TRACES_PRINT_INTERVAL4 = 50000;
    constexpr int COUNTABLE_TRACES_RESET_INTERVAL_MS = 600000; // 10 minutes

    // Exchange settings
    constexpr int KRAKEN_CHECKSUM_CHECK_PERIOD = 10; // check checksum every 100th update as it is slow

    // New configuration constants
    constexpr int SNAPSHOT_VALIDITY_CHECK_INTERVAL_MS = 1000;  // Check every second
    constexpr int SNAPSHOT_VALIDITY_CHECK_INTERVAL_PROLONGED_MS = 5000;
    constexpr int SNAPSHOT_VALIDITY_TIMEOUT_MS = 7000;
} 