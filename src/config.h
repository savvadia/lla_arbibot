#pragma once

namespace Config {
    // Event loop settings
    constexpr int EVENT_LOOP_DELAY_MS = 20;  // Delay between event loop iterations

    // Strategy settings
    constexpr int STRATEGY_CHECK_TIMER_MS = 5000;  // How often to check for opportunities

    // Best seen opportunity settings
    constexpr int BEST_SEEN_OPPORTUNITY_RESET_INTERVAL_MS = 60000;

    // Order book settings
    constexpr double MAX_ORDER_BOOK_AMOUNT = 100000.0;  // Maximum total amount to keep in order book

    // Execution settings
    constexpr int MAX_EXECUTION_TIME_MS = 0;  // Maximum time to run the bot (0 = run indefinitely)

    // min margin, %
    constexpr double MIN_MARGIN = 0.02;  // Minimum margin to execute a trade

    // countable traces print interval
    constexpr int COUNTABLE_TRACES_PRINT_INTERVAL = 100;

    // reset interval, ms
    constexpr int RESET_INTERVAL_MS = 600000; // 10 minutes
} 