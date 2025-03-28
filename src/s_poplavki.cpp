#include <iostream>
#include <string>
#include "balance.h"
#include "timers.h"
#include "s_poplavki.h"
#include <thread>
#include <iomanip>
#include <algorithm>
#include <sstream>
#include "tracer.h"
#include "types.h"

using namespace std;

// Define TRACE macro for StrategyPoplavki class
#define TRACE(...) TRACE_THIS(TraceInstance::STRAT, __VA_ARGS__)

Strategy::Strategy(std::string name, std::string coin, std::string stableCoin, TimersMgr &timersMgr) : name(name), coin(coin), stableCoin(stableCoin), timersMgr(timersMgr)
{
    balances = {};
}

void Strategy::setBalances(BalanceData balances)
{
    this->balances = balances;
    // validate that balances contain only the coins we need
    for (const auto &exchange : balances)
    {
        for (const auto &coin : exchange.second)
        {
            if (coin.first != this->coin && coin.first != this->stableCoin)
            {
                throw std::invalid_argument("Invalid coin in balances: " + coin.first + ", expected: " + this->coin + " or " + this->stableCoin);
            }
        }
    }
}

std::string Strategy::getName() const
{
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
    exchangeManager.getOrderBookManager().setUpdateCallback([this](ExchangeId exchangeId, TradingPair pair, const OrderBook& orderBook) {
        updateOrderBookData(exchangeId, orderBook);
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

void StrategyPoplavki::updateOrderBookData(ExchangeId exchange, const OrderBook& orderBook) {
    auto [bids, asks] = orderBook.getState();
    
    OrderBookData data;
    data.bestBid = bids.empty() ? 0.0 : bids[0].price;
    data.bestAsk = asks.empty() ? 0.0 : asks[0].price;
    data.bestBidQuantity = bids.empty() ? 0.0 : bids[0].quantity;
    data.bestAskQuantity = asks.empty() ? 0.0 : asks[0].quantity;
    data.lastUpdate = std::chrono::steady_clock::now();

    {
        std::lock_guard<std::mutex> lock(dataMutex);
        orderBookData[exchange] = data;
    }

    TRACE("Updated order book data for ", getExchange(exchange), 
          " bid:", data.bestBid, "(", data.bestBidQuantity, ")"
          " ask:", data.bestAsk, "(", data.bestAskQuantity, ")");
}

Opportunity StrategyPoplavki::calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange) {
    std::lock_guard<std::mutex> lock(dataMutex);
    const auto& buyData = orderBookData[buyExchange];
    const auto& sellData = orderBookData[sellExchange];

    if (buyData.bestAskQuantity  <= 0 || sellData.bestBidQuantity <= 0) {
        TRACE("No liquidity: ", buyData.bestAskQuantity, "@", getExchange(buyExchange), " ", sellData.bestBidQuantity, "@", getExchange(sellExchange));
        return Opportunity{buyExchange, sellExchange, 0.0, 0.0, 0.0};
    }
    if(buyData.bestAsk <= sellData.bestBid) {
        TRACE("No arbitrage: ", buyData.bestAsk, "@", getExchange(buyExchange), " ", sellData.bestBid, "@", getExchange(sellExchange));
        return Opportunity{buyExchange, sellExchange, 0.0, 0.0, 0.0};
    }
    double amount = std::min(buyData.bestAskQuantity, sellData.bestBidQuantity);
    return Opportunity{buyExchange, sellExchange, amount, buyData.bestAsk, sellData.bestBid};
}

void StrategyPoplavki::scanOpportunities() {
    TRACE("Starting opportunity scan...");
    
    // Check arbitrage between all pairs of exchanges
    for (size_t i = 0; i < exchangeIds.size(); ++i) {
        for (size_t j = i + 1; j < exchangeIds.size(); ++j) {
            ExchangeId exchange1 = exchangeIds[i];
            ExchangeId exchange2 = exchangeIds[j];

            auto opportunity1 = calculateProfit(exchange1, exchange2);
            if (opportunity1.amount > 0) {
                TRACE("ARBITRAGE OPPORTUNITY: ", opportunity1);
            }
            auto opportunity2 = calculateProfit(exchange2, exchange1);
            if (opportunity2.amount > 0) {
                TRACE("ARBITRAGE OPPORTUNITY: ", opportunity2);
            }
        }
    }
}

void StrategyPoplavki::execute() {
    TRACE("Executing Poplavki strategy...");
    scanOpportunities();
}

void StrategyPoplavki::timerCallback(int id, void *data) {
    auto* strategy = static_cast<StrategyPoplavki*>(data);
    if (strategy) {
        strategy->scanOpportunities();
        strategy->setScanInterval(1000);
    }
}

