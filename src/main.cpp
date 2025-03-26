#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <csignal>
#include "tracer.h"
#include "timers.h"
#include "ex_mgr.h"
#include "s_poplavki.h"
#include "balance.h"

using namespace std;

// Define TRACE macro for main
#define TRACE(...) TRACE_BASE(TraceInstance::MAIN, __VA_ARGS__)

// Global flag for graceful shutdown
volatile sig_atomic_t g_running = 1;

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    std::cout << std::endl << "Received signal " << signum << ", shutting down..." << std::endl;
    g_running = 0;
}

int main() {
    const int LOOP_SLEEP_MS = 500;
    TRACE("Starting LlaArbibot...");
    
    // Initialize tracing system
    FastTraceLogger::setLoggingEnabled(true);
    // FastTraceLogger::setLogFile("trading.log");
    
    // Enable specific trace types
    FastTraceLogger::setLoggingEnabled(TraceInstance::EVENT_LOOP, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::STRAT, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::BALANCE, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::ORDERBOOK, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::API, true);
    FastTraceLogger::setLoggingEnabled(TraceInstance::MAIN, true);
    TRACE("Trace types enabled: EVENT_LOOP, STRAT, BALANCE, ORDERBOOK, API, MAIN");

    // Set up signal handlers
    TRACE("Setting up signal handlers...");
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    // Create timer manager
    TRACE("Initializing TimersMgr...");
    TimersMgr timersMgr;
    
    // Create exchange manager
    TRACE("Initializing ExchangeManager...");
    ExchangeManager exchangeManager;
    
    // Define exchanges to use
    vector<ExchangeId> exchanges = {ExchangeId::KRAKEN, ExchangeId::BINANCE};
    TRACE("Using exchanges: ", exchanges[0], ", ", exchanges[1]);
    
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
    if (!exchangeManager.subscribeAllOrderBooks(TradingPair::BTC_USDT)) {
        TRACE("Failed to subscribe to order books");
        return 1;
    }

    // Get initial order book snapshots
    TRACE("Getting initial order book snapshots...");
    if (!exchangeManager.getOrderBookSnapshots(TradingPair::BTC_USDT)) {
        TRACE("Failed to get order book snapshots");
        return 1;
    }
    
    // Create strategy
    TRACE("Creating Poplavki strategy for BTC/USDT...");
    StrategyPoplavki strategy("BTC", "USDT", timersMgr, exchangeManager, exchanges);
    
    // Create balance manager
    TRACE("Initializing Balance manager...");
    Balance balance;
    
    // Initialize balances
    TRACE("Retrieving initial balances...");
    balance.retrieveBalances();
    
    // Set balances for strategy
    TRACE("Setting strategy balances...");
    strategy.setBalances(balance.getBalances());
    
    TRACE("System initialization complete, starting main loop...");
    
    // Main event loop
    int loopCount = 0;
    while (g_running && loopCount < 1000/LOOP_SLEEP_MS*5) {
        try {
            // Process timers
            timersMgr.checkTimers();
            
            // Execute strategy
            // strategy.execute();
            
            // Log loop count periodically
            if (++loopCount % 100 == 0) {
                TRACE("Main loop iteration: ", loopCount);
            }
            
            // Sleep for a short time to prevent CPU spinning
            std::this_thread::sleep_for(std::chrono::milliseconds(LOOP_SLEEP_MS));
        } catch (const std::exception& e) {
            TRACE("Error in main loop: ", e.what());
            break;
        }
    }
    
    TRACE("Trading system shutting down...");
    TRACE("Total main loop iterations: ", loopCount);

    // Proper cleanup sequence
    TRACE("Disconnecting from exchanges...");
    exchangeManager.disconnectAll();
    
    TRACE("Shutting down...");
    return 0;
}
