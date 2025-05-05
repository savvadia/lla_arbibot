#include <string>
#include <algorithm>

#include "config.h"
#include "balance.h"
#include "timers.h"
#include "s_poplavki.h"
#include "tracer.h"
#include "types.h"

using namespace std;

// Define TRACE macro for StrategyPoplavki class
#define TRACE(...) TRACE_THIS(TraceInstance::STRAT, ExchangeId::UNKNOWN, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::STRAT, ExchangeId::UNKNOWN, __VA_ARGS__)
#define TRACE_CNT(_id,...) TRACE_COUNT(TraceInstance::STRAT, _id, ExchangeId::UNKNOWN, __VA_ARGS__)

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
    
    // Set up periodic scanning
    TRACE("Setting up periodic scanning with ", Config::STRATEGY_CHECK_TIMER_MS, "ms interval");
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
    timersMgr.stopTimer(timerId);
    
    // Add new timer
    timerId = timersMgr.addTimer(ms, timerCallback, this, TimerType::PRICE_CHECK);
    DEBUG("Set up timer with ID ", timerId, " for scanning in ", ms, "ms");
}

void StrategyPoplavki::updateOrderBookData(ExchangeId exchange) {
    scanOpportunities();
}

Opportunity StrategyPoplavki::calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange) {
    auto& orderBookManager = exchangeManager.getOrderBookManager();
    auto buyBook = orderBookManager.getOrderBook(buyExchange, TradingPair::BTC_USDT);
    auto sellBook = orderBookManager.getOrderBook(sellExchange, TradingPair::BTC_USDT);

    double buyPrice = buyBook.getBestAsk();
    double sellPrice = sellBook.getBestBid();
    double amount  = std::min(buyBook.getBestAskQuantity(), sellBook.getBestBidQuantity());

    DEBUG("Calculating profit for ", 
        buyExchange, "(", buyBook.getLastUpdate(), ") -> ",
        sellExchange, "(", sellBook.getLastUpdate(), ") ",
        buyPrice, " -> ", sellPrice,
        " = ", (sellPrice - buyPrice), " (", (((sellPrice - buyPrice) / buyPrice) * 100), "%)");

    if (buyPrice > 0 && sellPrice > 0 && amount > 0 && buyPrice < sellPrice) {
        return Opportunity(buyExchange, sellExchange, amount, buyPrice, sellPrice);
    }

    return Opportunity(buyExchange, sellExchange, 0.0, 0.0, 0.0);
}

void StrategyPoplavki::scanOpportunities() {
    for (size_t i = 0; i < exchangeIds.size(); ++i) {
        for (size_t j = i + 1; j < exchangeIds.size(); ++j) {
            // Try both directions
            Opportunity opp1 = calculateProfit(exchangeIds[i], exchangeIds[j]);
            if (opp1.amount > 0 && opp1.profit() > Config::MIN_MARGIN) {
                TRACE_CNT(CountableTrace::S_POPLAVKI_OPPORTUNITY, "Found opportunity: ", opp1);
                // TODO: Execute opportunity
            }

            Opportunity opp2 = calculateProfit(exchangeIds[j], exchangeIds[i]);
            if (opp2.amount > 0 && opp2.profit() > Config::MIN_MARGIN) {
                TRACE_CNT(CountableTrace::S_POPLAVKI_OPPORTUNITY, "Found opportunity: ", opp2);
                // TODO: Execute opportunity
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
    strategy->startTimerToScan(Config::STRATEGY_CHECK_TIMER_MS);

    DEBUG_OBJ("INFO: ", strategy, TraceInstance::STRAT, ExchangeId::UNKNOWN, "Timer callback for strategy: ", strategy->getName());
    strategy->scanOpportunities();
}

