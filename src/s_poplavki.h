#pragma once

#include <string>
#include "balance.h"
#include "timers.h"
#include "orderbook.h"
#include <map>
#include <mutex>
#include "api_binance.h"
#include "api_kraken.h"
#include <memory>
#include <chrono>
#include "event_loop.h"

class Strategy
{
private:
    std::string name;
    std::string coin;       // coin to trade
    std::string stableCoin; // coin to trade against
    BalanceData balances;

protected:
    TimersMgr &timersMgr;

public:
    Strategy(std::string name, std::string coin, std::string stableCoin, TimersMgr &timersMgr);

    void setBalances(BalanceData balances);

    // Core strategy methods
    virtual void execute() = 0;
    
    // Optional methods for exchange updates
    virtual void onExchangeUpdate(ExchangeId exchange) {}
    
    // Optional methods for periodic scanning
    virtual void scanOpportunities() {}
    virtual void setScanInterval(int ms) {}
    
    virtual ~Strategy() = default;
    std::string getName() const;
};

class StrategyPoplavki : public Strategy
{
public:
    StrategyPoplavki(const std::string& baseAsset, const std::string& quoteAsset, TimersMgr& timers);
    ~StrategyPoplavki() override;

    // Event handling
    void onExchangeUpdate(ExchangeId exchange) override;
    
    // Scanning
    void scanOpportunities() override;
    void setScanInterval(int ms) override;

    static void timerCallback(int id, void *data);
    void execute() override;

private:
    // Exchange data
    struct OrderBookData {
        double bestBid = 0.0;
        double bestAsk = 0.0;
        double bestBidQuantity = 0.0;
        double bestAskQuantity = 0.0;
        std::chrono::steady_clock::time_point lastUpdate;
    };

    // Core components
    std::string baseAsset;
    std::string quoteAsset;
    OrderBookManager orderBookManager;
    
    // Exchange instances
    std::unique_ptr<BinanceApi> binanceApi;
    std::unique_ptr<KrakenApi> krakenApi;

    // Order book data
    std::mutex dataMutex;
    std::map<ExchangeId, OrderBookData> orderBookData;
    int timerId = -1;  // Store the timer identifier

    // Helper methods
    void updateOrderBookData(ExchangeId exchange, const OrderBook& orderBook);
    void checkArbitrage();
    double calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange, double amount);
    void disconnectExchanges();
};