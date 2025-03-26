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
    std::lock_guard<std::mutex> lock(dataMutex);
    
    OrderBookData& data = orderBookData[exchange];
    data.bestBid = orderBook.getBestBid();
    data.bestAsk = orderBook.getBestAsk();
    data.bestBidQuantity = orderBook.getBestBidQuantity();
    data.bestAskQuantity = orderBook.getBestAskQuantity();
    data.lastUpdate = std::chrono::steady_clock::now();

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2) << data.bestBid 
        << " (" << std::setprecision(4) << data.bestBidQuantity << ") "
        << "ask: " << std::setprecision(2) << data.bestAsk 
        << " (" << std::setprecision(4) << data.bestAskQuantity << ")";
    
    TRACE("Updated order book for ", exchange, " bid: ", oss.str());
}

void StrategyPoplavki::scanOpportunities() {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    // Initialize data for all exchanges if not present
    for (const auto& exchangeId : exchangeIds) {
        if (orderBookData.find(exchangeId) == orderBookData.end()) {
            TRACE("Initializing order book data for ", *exchangeManager.getExchange(exchangeId));
            orderBookData[exchangeId] = OrderBookData{};
        }
    }

    // Check arbitrage between all pairs of exchanges
    for (size_t i = 0; i < exchangeIds.size(); ++i) {
        for (size_t j = i + 1; j < exchangeIds.size(); ++j) {
            ExchangeId exchange1 = exchangeIds[i];
            ExchangeId exchange2 = exchangeIds[j];
            
            const auto& data1 = orderBookData[exchange1];
            const auto& data2 = orderBookData[exchange2];

            // Check if we have valid data from both exchanges
            if (data1.bestBid == 0 || data1.bestAsk == 0 || 
                data2.bestBid == 0 || data2.bestAsk == 0) {
                TRACE("No valid data for pair ", *exchangeManager.getExchange(exchange1), " - ", *exchangeManager.getExchange(exchange2));
                continue;  // Skip this pair if no valid data
            }

            // Check exchange1 -> exchange2 arbitrage
            if (data1.bestAsk < data2.bestBid) {
                double amount = std::min(data1.bestAskQuantity, data2.bestBidQuantity);
                double profit = calculateProfit(exchange1, exchange2, amount);
                
                if (profit > 0) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << data1.bestAsk 
                        << " -> " << data2.bestBid << " amount " << amount 
                        << " profit " << profit;
                    TRACE("ARBITRAGE OPPORTUNITY: ", *exchangeManager.getExchange(exchange1), " -> ", *exchangeManager.getExchange(exchange2), ": ", oss.str());
                }
            }
            
            // Check exchange2 -> exchange1 arbitrage
            if (data2.bestAsk < data1.bestBid) {
                double amount = std::min(data2.bestAskQuantity, data1.bestBidQuantity);
                double profit = calculateProfit(exchange2, exchange1, amount);
                
                if (profit > 0) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(2) << data2.bestAsk 
                        << " -> " << data1.bestBid << " amount " << amount 
                        << " profit " << profit;
                    TRACE("ARBITRAGE OPPORTUNITY: ", *exchangeManager.getExchange(exchange2), " -> ", *exchangeManager.getExchange(exchange1), ": ", oss.str());
                }
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

double StrategyPoplavki::calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange, double amount) {
    const auto& buyData = orderBookData[buyExchange];
    const auto& sellData = orderBookData[sellExchange];
    
    double buyCost = amount * buyData.bestAsk;
    double sellRevenue = amount * sellData.bestBid;
    
    return sellRevenue - buyCost;
}
