#pragma once

namespace Config {
    // Event loop settings
    constexpr int EVENT_LOOP_DELAY_MS = 500;  // Delay between event loop iterations

    // Strategy settings
    constexpr int STRATEGY_CHECK_TIMER_MS = 3000;  // How often to check for opportunities

    // Order book settings
    constexpr double MAX_ORDER_BOOK_AMOUNT = 100000.0;  // Maximum total amount to keep in order book

    // Execution settings
    constexpr int MAX_EXECUTION_TIME_MS = 0;  // Maximum time to run the bot (0 = run indefinitely)
} 