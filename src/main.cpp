#include <iostream>
#include <iomanip>
#include <memory>
#include <thread>
#include <chrono>

#include "balance.h"
#include "timers.h"
#include "s_poplavki.h"

using namespace std;

int main() {
    try {
        // Initialize components
        Balance balance;
        TimersMgr timers;
        
        // Create strategy (this will connect to exchanges and subscribe to order books)
        auto strategy = std::make_unique<StrategyPoplavki>("BTC", "USDT", timers);
        
        // Get initial balances
        balance.getBalances();
        
        // Main loop
        while (true) {
            timers.checkTimers();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
