#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <csignal>
#include "tracer.h"
#include "timers.h"
#include "ex_mgr.h"
#include "s_poplavki.h"
#include "balance.h"
#include "config.h"
#include "tracer_timer.h"
using namespace std;

// Define TRACE macro for main
#define TRACE(...) TRACE_BASE(TraceInstance::MAIN, ExchangeId::UNKNOWN, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::MAIN, ExchangeId::UNKNOWN, __VA_ARGS__)
// Global flag for graceful shutdown
volatile sig_atomic_t g_running = 1;
std::atomic<bool> g_shutdown_requested{false};

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    if (!g_shutdown_requested) {
        std::cout << std::endl << "Received signal " << signum << ", initiating graceful shutdown..." << std::endl;
        g_shutdown_requested = true;
        g_running = 0;
    } else {
        std::cout << std::endl << "Forcing immediate shutdown..." << std::endl;
        exit(1);
    }
}

int main() {
    const int SHUTDOWN_TIMEOUT_MS = 5000; // 5 seconds timeout for graceful shutdown
    TRACE("Starting LlaArbibot...");
    
    // Initialize tracing system
    FastTraceLogger::setLoggingEnabled(true);
    // FastTraceLogger::setLogFile("trading.log");
    
    // Enable specific trace types
    FastTraceLogger::setLoggingEnabled(TraceInstance::EVENT_LOOP, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::TRACES, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::TIMER, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::STRAT, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::BALANCE, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::ORDERBOOK, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::ORDERBOOK_MGR, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::A_EXCHANGE, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::A_IO, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::A_KRAKEN, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::A_BINANCE, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::A_KUCOIN, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::A_OKX, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::A_CRYPTO, false);
    FastTraceLogger::setLoggingEnabled(TraceInstance::MAIN, true);

    // Enable exchange-specific logging
    FastTraceLogger::setLoggingEnabled(ExchangeId::UNKNOWN, true);
    FastTraceLogger::setLoggingEnabled(ExchangeId::BINANCE, false);
    FastTraceLogger::setLoggingEnabled(ExchangeId::KRAKEN, false);
    FastTraceLogger::setLoggingEnabled(ExchangeId::KUCOIN, false);
    FastTraceLogger::setLoggingEnabled(ExchangeId::OKX, false);
    FastTraceLogger::setLoggingEnabled(ExchangeId::CRYPTO, false);   
    TRACE("Trace types enabled: EVENT_LOOP, TRACES, TIMER, STRAT, BALANCE, ORDERBOOK, A_EXCHANGE, A_IO, A_KRAKEN, A_BINANCE, A_KUCOIN, A_OKX, A_CRYPTO, MAIN");
    TRACE("Exchange logging enabled: BINANCE, KRAKEN, KUCOIN, OKX, CRYPTO");

    // Set up signal handlers
    TRACE("Setting up signal handlers...");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create timer manager
    TRACE("Initializing TimersMgr...");
    TimersMgr timersMgr;
    OrderBookManager orderBookManager;

    // init reset countable traces timer
    initResetCountableTracesTimer(timersMgr);
    
    // Create exchange manager
    TRACE("Initializing ExchangeManager...");
    std::vector<TradingPair> pairs;
    for (int i = 0; i < static_cast<int>(TradingPair::COUNT); i++) {
        if (i == static_cast<int>(TradingPair::UNKNOWN)) continue;
        pairs.push_back(static_cast<TradingPair>(i));
    }
    ExchangeManager exchangeManager(timersMgr, orderBookManager, pairs);
    
    // Define exchanges to use
    vector<ExchangeId> exchanges;
    std::ostringstream exchangesStr;
    for (int i = 0; i < static_cast<int>(ExchangeId::COUNT); i++) {
        if (i == static_cast<int>(ExchangeId::UNKNOWN)) continue;
        if (i == static_cast<int>(ExchangeId::BINANCE)) continue;
        if (i == static_cast<int>(ExchangeId::KRAKEN)) continue;
        exchanges.push_back(static_cast<ExchangeId>(i));
        exchangesStr << static_cast<ExchangeId>(i) << ", ";
    }
    TRACE("Using exchanges: ", exchangesStr.str());
    
    // Initialize exchanges
    TRACE("Initializing exchanges...");
    if (!exchangeManager.initializeExchanges(exchanges)) {
        TRACE("Failed to initialize exchanges");
        return 1;
    }

    // Connect to exchanges
    TRACE("Connecting to exchanges...");
    if (!exchangeManager.connectAll()) {
        TRACE("Failed to connect to exchanges");
        return 1;
    }

    // Subscribe to order books
    TRACE("Subscribing to order books...");
    if (!exchangeManager.subscribeAllOrderBooks()) {
        TRACE("Failed to subscribe to order books");
        return 1;
    }

    // Get initial order book snapshots with timeout
    auto startTime = std::chrono::steady_clock::now();

    // Create balance manager
    TRACE("Initializing Balance manager...");
    Balance balance;
    
    // Initialize balances
    TRACE("Retrieving initial balances...");
    balance.retrieveBalances();

    TRACE("Initializing strategies...");
    vector<StrategyPoplavki*> strategies;
    for (int i = 0; i < static_cast<int>(TradingPair::COUNT); i++) {
        if (i == static_cast<int>(TradingPair::UNKNOWN)) continue;
        TradingPair pair = static_cast<TradingPair>(i);
        TradingPairCoins coins = getTradingPairCoins(pair);
        strategies.emplace_back(new StrategyPoplavki(
            coins.base, coins.quote, pair, timersMgr, exchangeManager, exchanges));

        strategies.back()->setBalances(balance.getBalances());
    }

    TRACE("System initialization complete, starting main loop...");
    
    // Main event loop
    int loopCount = 0;
    startTime = std::chrono::steady_clock::now();
    auto shutdownStartTime = std::chrono::steady_clock::now();
    
    while (g_running) {
        try {
            // Check if we've exceeded the maximum execution time
            if (Config::MAX_EXECUTION_TIME_MS > 0) {
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::steady_clock::now() - startTime).count();
                if (elapsed >= Config::MAX_EXECUTION_TIME_MS) {
                    TRACE("Maximum execution time reached (", Config::MAX_EXECUTION_TIME_MS, "ms)");
                    break;
                }
            }
            
            // Process timers
            timersMgr.checkTimers();
            
            // Execute strategy
            // TODO: check if we need it. scan is done either on timer or on exchange update
            // strategy->execute();
            
            // Log loop count periodically
            if (++loopCount % 1000 == 0) {
                TRACE("Main loop iteration: ", loopCount);
            }
            
            // Check if shutdown was requested
            if (g_shutdown_requested) {
                auto now = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - shutdownStartTime).count();
                
                if (elapsed >= SHUTDOWN_TIMEOUT_MS) {
                    TRACE("Shutdown timeout reached, forcing immediate shutdown");
                    break;
                }
                
                // During graceful shutdown, try to disconnect from exchanges
                TRACE("Attempting graceful shutdown...");
                exchangeManager.disconnectAll();
            }
            
            // Sleep for a short time to prevent CPU spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(Config::EVENT_LOOP_DELAY_MS));
        } catch (const std::exception& e) {
            ERROR("Error in main loop: ", e.what());
            // Don't break on error, try to continue
            std::this_thread::sleep_for(std::chrono::milliseconds(Config::EVENT_LOOP_DELAY_MS));
        }
    }
    
    TRACE("Trading system shutting down...");
    TRACE("Total main loop iterations: ", loopCount);

    // Final cleanup
    TRACE("Disconnecting from exchanges...");
    exchangeManager.disconnectAll();
    
    TRACE("Shutting down...");
    return 0;
}
