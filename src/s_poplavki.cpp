#include <iostream>
#include <string>
#include <thread>
#include <iomanip>
#include <algorithm>
#include <sstream>

#include "config.h"
#include "balance.h"
#include "timers.h"
#include "s_poplavki.h"
#include "tracer.h"
#include "types.h"

using namespace std;

// Define TRACE macro for StrategyPoplavki class
#define TRACE(...) TRACE_THIS(TraceInstance::STRAT, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::STRAT, __VA_ARGS__)

Strategy::Strategy(std::string name, std::string coin, std::string stableCoin, TimersMgr &timersMgr) 
    : name(name), coin(coin), stableCoin(stableCoin), timersMgr(timersMgr) {
    balances = {};
}

void Strategy::setBalances(BalanceData balances) {
    this->balances = balances;
    // validate that balances contain only the coins we need
    for (const auto &exchange : balances) {
        for (const auto &coin : exchange.second) {
            if (coin.first != this->coin && coin.first != this->stableCoin) {
                throw std::invalid_argument("Invalid coin in balances: " + coin.first + ", expected: " + this->coin + " or " + this->stableCoin);
            }
        }
    }
}

std::string Strategy::getName() const {
    return name;
}

StrategyPoplavki::StrategyPoplavki(const std::string& baseAsset, 
                                 const std::string& quoteAsset, 
                                 TimersMgr& timers,
                                 ExchangeManager& exchangeManager,
                                 const std::vector<ExchangeId>& exchangeIds)
    : Strategy("Poplavki", baseAsset, quoteAsset, timers)
    , baseAsset(baseAsset)
    , quoteAsset(quoteAsset)
    , exchangeManager(exchangeManager)
    , exchangeIds(exchangeIds) {
    
    TRACE("Initializing with ", exchangeIds.size(), " exchanges");
    
    // Set up order book update callback
    exchangeManager.getOrderBookManager().setUpdateCallback([this](ExchangeId exchangeId, TradingPair pair) {
        updateOrderBookData(exchangeId);
    });
    
    // Set up periodic scanning (3 seconds)
    TRACE("Setting up periodic scanning with 1 second interval");
    setScanInterval(1000);
}

StrategyPoplavki::~StrategyPoplavki() {
    // Nothing to clean up - ExchangeManager handles exchange lifecycle
}

void StrategyPoplavki::onExchangeUpdate(ExchangeId exchange) {
    TRACE("Received update from exchange: ", exchange);
    // Optional: React immediately to exchange updates
    scanOpportunities();
}

void StrategyPoplavki::setScanInterval(int ms) {
    // Remove existing timer if any
    timersMgr.stopTimer(timerId);
    
    // Add new timer
    timerId = timersMgr.addTimer(ms, timerCallback, this, TimerType::PRICE_CHECK);
    TRACE("Set up timer with ID ", timerId, " for scanning in ", ms, "ms");
}

void StrategyPoplavki::updateOrderBookData(ExchangeId exchange) {
    TRACE("Received order book update from exchange: ", exchange);
    scanOpportunities();
}

Opportunity StrategyPoplavki::calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange) {
    auto& orderBookManager = exchangeManager.getOrderBookManager();
    auto buyBook = orderBookManager.getOrderBook(buyExchange, TradingPair::BTC_USDT);
    auto sellBook = orderBookManager.getOrderBook(sellExchange, TradingPair::BTC_USDT);

    double buyPrice = buyBook.getBestAsk();
    double sellPrice = sellBook.getBestBid();
    double profit = sellPrice - buyPrice;
    double profitPercent = (profit / buyPrice) * 100;
    double amount  = std::min(buyBook.getBestAskQuantity(), sellBook.getBestBidQuantity());

    TRACE("Calculating profit for ", 
        buyExchange, "(", buyBook.getLastUpdate(), ") -> ",
        sellExchange, "(", sellBook.getLastUpdate(), ") ",
        buyPrice, " -> ", sellPrice,
        " = ", profit, " (", profitPercent, "%)");

    if (buyPrice > 0 && sellPrice > 0 && amount > 0 && buyPrice < sellPrice) {
        return Opportunity(buyExchange, sellExchange, amount, buyPrice, sellPrice);
    }

    return Opportunity(buyExchange, sellExchange, 0.0, 0.0, 0.0);
}

void StrategyPoplavki::scanOpportunities() {
    DEBUG("Starting opportunity scan...");
    
    for (size_t i = 0; i < exchangeIds.size(); ++i) {
        for (size_t j = i + 1; j < exchangeIds.size(); ++j) {
            // Try both directions
            Opportunity opp1 = calculateProfit(exchangeIds[i], exchangeIds[j]);
            if (opp1.amount > 0) {
                TRACE("Found opportunity: ", opp1);
                // TODO: Execute opportunity
            }

            Opportunity opp2 = calculateProfit(exchangeIds[j], exchangeIds[i]);
            if (opp2.amount > 0) {
                TRACE("Found opportunity: ", opp2);
                // TODO: Execute opportunity
            }
        }
    }
}

void StrategyPoplavki::execute() {
    scanOpportunities();
}

void StrategyPoplavki::timerCallback(int id, void *data) {
    auto* strategy = static_cast<StrategyPoplavki*>(data);
    strategy->scanOpportunities();
}

