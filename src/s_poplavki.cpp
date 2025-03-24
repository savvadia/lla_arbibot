#include <iostream>
#include <string>
#include "balance.h"
#include "timers.h"
#include "s_poplavki.h"
#include <thread>
#include <iomanip>
#include <algorithm>

using namespace std;

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

StrategyPoplavki::StrategyPoplavki(const std::string& baseAsset, const std::string& quoteAsset, TimersMgr& timers)
    : Strategy("Poplavki", baseAsset, quoteAsset, timers)
    , baseAsset(baseAsset)
    , quoteAsset(quoteAsset) {
    
    // Create exchange instances
    binanceApi = std::make_unique<BinanceApi>(orderBookManager);
    krakenApi = std::make_unique<KrakenApi>(orderBookManager);
    
    // Set up order book update callback
    orderBookManager.setUpdateCallback([this](TradingPair pair, const OrderBook& orderBook) {
        // TODO: Refactor to use a single exchange instance that can handle multiple exchanges
        // This would eliminate the need for separate API instances and conditional logic
        // Current implementation unnecessarily splits the same functionality across different objects
        if (binanceApi) {
            updateOrderBookData(binanceApi->getExchangeId(), orderBook);
        } else if (krakenApi) {
            updateOrderBookData(krakenApi->getExchangeId(), orderBook);
        }
    });
    
    // Connect to exchanges
    if (!binanceApi->connect()) {
        throw std::runtime_error("Failed to connect to Binance");
    }
    
    if (!krakenApi->connect()) {
        throw std::runtime_error("Failed to connect to Kraken");
    }
    
    // Subscribe to order books
    binanceApi->subscribeOrderBook(TradingPair::BTC_USDT);
    krakenApi->subscribeOrderBook(TradingPair::BTC_USDT);
    
    // Set up periodic scanning (default 1 second)
    setScanInterval(1000);
}

StrategyPoplavki::~StrategyPoplavki() {
    disconnectExchanges();
}

void StrategyPoplavki::disconnectExchanges() {
    if (binanceApi) {
        binanceApi->disconnect();
    }
    if (krakenApi) {
        krakenApi->disconnect();
    }
}

void StrategyPoplavki::onExchangeUpdate(ExchangeId exchange) {
    // Optional: React immediately to exchange updates
    // This could be useful for very time-sensitive opportunities
    scanOpportunities();
}

void StrategyPoplavki::scanOpportunities() {
    checkArbitrage();
}

void StrategyPoplavki::setScanInterval(int ms) {
    // Remove existing timer if any
    timersMgr.stopTimer(timerId);
    
    // Add new timer
    timerId = timersMgr.addTimer(ms, timerCallback, this, TimerType::PRICE_CHECK);
}

void StrategyPoplavki::updateOrderBookData(ExchangeId exchange, const OrderBook& orderBook) {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    OrderBookData& data = orderBookData[exchange];
    data.bestBid = orderBook.getBestBid();
    data.bestAsk = orderBook.getBestAsk();
    data.bestBidQuantity = orderBook.getBestBidQuantity();
    data.bestAskQuantity = orderBook.getBestAskQuantity();
    data.lastUpdate = std::chrono::steady_clock::now();
}

void StrategyPoplavki::checkArbitrage() {
    std::lock_guard<std::mutex> lock(dataMutex);
    
    // Check if we have data from both exchanges
    if (orderBookData.size() < 2) {
        return;
    }

    // Get data for both exchanges
    const auto& binanceData = orderBookData[ExchangeId::BINANCE];
    const auto& krakenData = orderBookData[ExchangeId::KRAKEN];

    // Check Binance -> Kraken arbitrage
    if (binanceData.bestAsk < krakenData.bestBid) {
        double amount = std::min(binanceData.bestAskQuantity, krakenData.bestBidQuantity);
        double profit = calculateProfit(ExchangeId::BINANCE, ExchangeId::KRAKEN, amount);
        
        if (profit > 0) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Binance -> Kraken: " 
                      << binanceData.bestAsk << " -> " << krakenData.bestBid 
                      << " amount " << amount 
                      << " profit " << profit << std::endl;
        }
    }
    
    // Check Kraken -> Binance arbitrage
    if (krakenData.bestAsk < binanceData.bestBid) {
        double amount = std::min(krakenData.bestAskQuantity, binanceData.bestBidQuantity);
        double profit = calculateProfit(ExchangeId::KRAKEN, ExchangeId::BINANCE, amount);
        
        if (profit > 0) {
            std::cout << std::fixed << std::setprecision(2);
            std::cout << "Kraken -> Binance: " 
                      << krakenData.bestAsk << " -> " << binanceData.bestBid 
                      << " amount " << amount 
                      << " profit " << profit << std::endl;
        }
    }
}

double StrategyPoplavki::calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange, double amount) {
    const auto& buyData = orderBookData[buyExchange];
    const auto& sellData = orderBookData[sellExchange];
    
    double buyCost = amount * buyData.bestAsk;
    double sellRevenue = amount * sellData.bestBid;
    
    return sellRevenue - buyCost;
}

void StrategyPoplavki::timerCallback(int id, void* data) {
    auto* strategy = static_cast<StrategyPoplavki*>(data);
    strategy->scanOpportunities();
}

void StrategyPoplavki::execute() {
    scanOpportunities();
}
