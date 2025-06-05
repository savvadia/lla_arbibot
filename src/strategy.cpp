#include "strategy.h"
#include "tracer.h"
#include "timers.h"
#include "config.h"

// Define TRACE macro for StrategyPoplavki class
#define TRACE(...) TRACE_THIS(TraceInstance::STRAT, ExchangeId::UNKNOWN, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::STRAT, ExchangeId::UNKNOWN, __VA_ARGS__)
#define TRACE_CNT(_id,...) TRACE_COUNT(TraceInstance::STRAT, _id, ExchangeId::UNKNOWN, __VA_ARGS__)
#define ERROR_CNT(_id, _exchangeId, ...) ERROR_COUNT(TraceInstance::STRAT, _id, _exchangeId, __VA_ARGS__)

Strategy::Strategy(std::string name, std::string coin, std::string stableCoin, TradingPair pair) 
    : balances({}), name(name), coin(coin), stableCoin(stableCoin), pair(pair),
    bestOpportunity1(ExchangeId::UNKNOWN, ExchangeId::UNKNOWN, pair, 0.0, 0.0, 0.0, std::chrono::system_clock::now()),
    bestOpportunity2(ExchangeId::UNKNOWN, ExchangeId::UNKNOWN, pair, 0.0, 0.0, 0.0, std::chrono::system_clock::now())
    {
            
    timersManager.addTimer(Config::BEST_SEEN_OPPORTUNITY_RESET_INTERVAL_MS,
        &Strategy::resetBestSeenOpportunityTimerCallback, this,
        TimerType::RESET_BEST_SEEN_OPPORTUNITY, true);
}

void Strategy::setBalances(BalanceData balances) {
    this->balances = balances;
    // validate that balances contain only the coins we need
    for (const auto &exchange : balances) {
        for (const auto &coin : exchange.second) {
            if (coin.first != this->coin && coin.first != this->stableCoin) {
                DEBUG("Ignored coin in balances: " + coin.first + ", expected: " + this->coin + " or " + this->stableCoin);
            }
        }
    }
}

std::string Strategy::getName() const {
    return name;
}
