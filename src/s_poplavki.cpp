#include <string>
#include <algorithm>

#include "config.h"
#include "timers.h"
#include "s_poplavki.h"
#include "tracer.h"
#include "types.h"
#include "orderbook_mgr.h"
#include "order_mgr.h"

using namespace std;

// Define TRACE macro for StrategyPoplavki class
#define TRACE(...) TRACE_THIS(TraceInstance::STRAT, ExchangeId::UNKNOWN, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::STRAT, ExchangeId::UNKNOWN, __VA_ARGS__)
#define TRACE_CNT(_id,...) TRACE_COUNT(TraceInstance::STRAT, _id, ExchangeId::UNKNOWN, __VA_ARGS__)
#define ERROR_CNT(_id, _exchangeId, ...) ERROR_COUNT(TraceInstance::STRAT, _id, _exchangeId, __VA_ARGS__)

StrategyPoplavki::StrategyPoplavki(const std::string& baseAsset, 
                                 const std::string& quoteAsset, 
                                 TradingPair pair,
                                 const std::vector<ExchangeId>& exchangeIds)
    : Strategy("Poplavki", baseAsset, quoteAsset, pair)
    , baseAsset(baseAsset)
    , quoteAsset(quoteAsset)
    , exchangeIds(exchangeIds) {
    
    TRACE("Initializing with ", exchangeIds.size(), " exchanges");
    
    // Set up order book update callback
    orderBookManager.setUpdateCallback([this](ExchangeId exchangeId, TradingPair pair) {
        updateOrderBookData(exchangeId);
    });
    
    // Set up periodic scanning
    DEBUG("Setting up periodic scanning with ", Config::STRATEGY_CHECK_TIMER_MS, "ms interval");
    startTimerToScan(Config::STRATEGY_CHECK_TIMER_MS);
}

StrategyPoplavki::~StrategyPoplavki() {
    // Nothing to clean up - ExchangeManager handles exchange lifecycle
}

void StrategyPoplavki::onExchangeUpdate(ExchangeId exchange) {
    TRACE("Received update from exchange: ", exchange);
    // Optional: React immediately to exchange updates
    scanOpportunities();
}

void StrategyPoplavki::startTimerToScan(int ms) {
    // Remove existing timer if any
    timersManager.stopTimer(timerId);
    
    // Add new timer
    timerId = timersManager.addTimer(ms, timerCallback, this, TimerType::PRICE_CHECK, true);
    DEBUG("Set up timer with ID ", timerId, " for scanning in ", ms, "ms");
}

void StrategyPoplavki::updateOrderBookData(ExchangeId exchange) {
    scanOpportunities();
}

Opportunity StrategyPoplavki::calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange, TradingPair pair) {
    auto buyBook = orderBookManager.getOrderBook(buyExchange, pair);
    auto sellBook = orderBookManager.getOrderBook(sellExchange, pair);

    double buyPrice = buyBook.getBestAsk();
    double sellPrice = sellBook.getBestBid();
    double amount  = std::min(buyBook.getBestAskQuantity(), sellBook.getBestBidQuantity());

    DEBUG("Calculating profit for ", 
        buyExchange, "(", buyBook.getLastUpdate(), ") -> ",
        sellExchange, "(", sellBook.getLastUpdate(), ") ",
        buyPrice, " -> ", sellPrice,
        " = ", (sellPrice - buyPrice), " (", (((sellPrice - buyPrice) / buyPrice) * 100), "%)");

    if(buyPrice * 2 < sellPrice || sellPrice * 2 < buyPrice) {
        ERROR_CNT(CountableTrace::S_POPLAVKI_OPPORTUNITY_PRICE_DIFF, buyPrice < sellPrice ? buyExchange : sellExchange,
            "Major price difference: ",
            buyPrice, " at ", buyExchange, " (", buyBook.getLastUpdate(), ") -> ",
            sellPrice, " at ", sellExchange, " (", sellBook.getLastUpdate(), ")");
        return Opportunity(buyExchange, sellExchange, TradingPair::UNKNOWN, 0.0, 0.0, 0.0, std::chrono::system_clock::now());
    }

    if (buyPrice > 0 && sellPrice > 0 && amount > 0 && buyPrice < sellPrice) {
        return Opportunity(buyExchange, sellExchange, pair, amount, buyPrice, sellPrice, std::chrono::system_clock::now());
    }

    return Opportunity(buyExchange, sellExchange, TradingPair::UNKNOWN, 0.0, 0.0, 0.0, std::chrono::system_clock::now());
}

void StrategyPoplavki::scanOpportunities() {
    for (size_t i = 0; i < exchangeIds.size(); ++i) {
        for (size_t j = i + 1; j < exchangeIds.size(); ++j) {
            // Try both directions
            Opportunity opp1 = calculateProfit(exchangeIds[i], exchangeIds[j], pair);
            if (opp1.amount > 0 && opp1.profit() > Config::MIN_TRACEABLE_MARGIN) {
                DEBUG("Found opportunity: ", opp1);
                // TODO: Execute opportunity
                if (bestOpportunity1.amount == 0 || opp1.profit() > bestOpportunity1.profit()) {
                    TRACE_CNT(CountableTrace::S_POPLAVKI_OPPORTUNITY, "Updating best opp1: ", opp1);
                    bestOpportunity1 = opp1;
                } else {
                    DEBUG("Best seen opportunity is better: ", bestOpportunity1, " vs ", opp1);
                }
                if (bestOpportunity1.profit() > Config::MIN_EXECUTION_MARGIN) {
                    TRACE_CNT(CountableTrace::S_POPLAVKI_OPPORTUNITY_EXECUTABLE, "EXECUTABLE: ", bestOpportunity1);
                    orderManager.handleOpportunity(bestOpportunity1);
                }
            }

            Opportunity opp2 = calculateProfit(exchangeIds[j], exchangeIds[i], pair);
            if (opp2.amount > 0 && opp2.profit() > Config::MIN_TRACEABLE_MARGIN) {
                DEBUG("Found opportunity: ", opp2);
                if (bestOpportunity2.amount == 0 || opp2.profit() > bestOpportunity2.profit()) {
                    TRACE_CNT(CountableTrace::S_POPLAVKI_OPPORTUNITY, "Updating best opp2: ", opp2);
                    bestOpportunity2 = opp2;
                } else {
                    DEBUG("Best seen opportunity is better: ", bestOpportunity2, " vs ", opp2);
                }
                if (bestOpportunity2.profit() > Config::MIN_EXECUTION_MARGIN) {
                    TRACE_CNT(CountableTrace::S_POPLAVKI_OPPORTUNITY_EXECUTABLE, "EXECUTABLE: ", bestOpportunity2);
                    orderManager.handleOpportunity(bestOpportunity2);
                }
            }
        }
    }
}

void StrategyPoplavki::execute() {
    TRACE("Executing strategy...");
    scanOpportunities();
}

void StrategyPoplavki::timerCallback(int id, void *data) {
    auto* strategy = static_cast<StrategyPoplavki*>(data);
    
    DEBUG_OBJ("INFO: ", strategy, TraceInstance::STRAT, ExchangeId::UNKNOWN, "Timer callback for strategy: ", strategy->getName());
    strategy->scanOpportunities();
}

// Make the callback function a static member of Strategy
void Strategy::resetBestSeenOpportunityTimerCallback(int id, void *data) {
    auto* strategy = static_cast<StrategyPoplavki*>(data);
    if (strategy->bestOpportunity1.amount > 0) {
        TRACE_BASE(TraceInstance::STRAT, ExchangeId::UNKNOWN,
            "Resetting best seen opportunity1 for ", strategy->getName(), " ", strategy->pair, ": ", strategy->bestOpportunity1);
        strategy->bestOpportunity1 = Opportunity(ExchangeId::UNKNOWN, ExchangeId::UNKNOWN, strategy->pair, 0, 0, 0, std::chrono::system_clock::now());
    }
    if (strategy->bestOpportunity2.amount > 0) {
        TRACE_BASE(TraceInstance::STRAT, ExchangeId::UNKNOWN,
            "Resetting best seen opportunity2 for ", strategy->getName(), " ", strategy->pair, ": ", strategy->bestOpportunity2);
        strategy->bestOpportunity2 = Opportunity(ExchangeId::UNKNOWN, ExchangeId::UNKNOWN, strategy->pair, 0, 0, 0, std::chrono::system_clock::now());
    }
}

