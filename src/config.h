#pragma once

namespace Config {
    // Execution settings
    constexpr int EVENT_LOOP_DELAY_MS = 20;  // Delay between event loop iterations
    constexpr int MAX_EXECUTION_TIME_MS = 0;  // Maximum time to run the bot (0 = run indefinitely)

    // Strategy settings
    constexpr int STRATEGY_CHECK_TIMER_MS = 5000;  // How often to check for opportunities
    constexpr int BEST_SEEN_OPPORTUNITY_RESET_INTERVAL_MS = 600000; // 10 minutes
    constexpr double MIN_MARGIN = 0.21;  // Minimum margin to execute a trade, %
    constexpr double MIN_TRACEABLE_MARGIN = 0.02;  // Minimum margin to trace, %

    // Traces settings
    constexpr int COUNTABLE_TRACES_PRINT_INTERVAL = 10;
    constexpr int RESET_INTERVAL_MS = 600000; // 10 minutes

    // Exchange settings
    constexpr int KRAKEN_CHECKSUM_CHECK_PERIOD = 10; // check checksum every 100th update as it is slow

    // New configuration constants
    constexpr int SNAPSHOT_VALIDITY_CHECK_INTERVAL_MS = 1000;  // Check every second
    constexpr int SNAPSHOT_VALIDITY_TIMEOUT_MS = 7000;  // Snapshot is invalid after 5 seconds
} 