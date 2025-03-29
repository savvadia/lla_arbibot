#pragma once

#include <string>
#include "balance.h"
#include "timers.h"
#include "orderbook.h"
#include <map>
#include <mutex>
#include <vector>
#include <memory>
#include <chrono>
#include "event_loop.h"
#include "ex_mgr.h"
#include "tracer.h"
#include <iomanip>

class Strategy : public Traceable
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

    // For TRACE identification
    const std::string &getStrategyName() const { return name; }

protected:
    virtual void trace(std::ostream &os) const override
    {
        os << name << " " << coin << "/" << stableCoin;
    }
};

class Opportunity : public Traceable
{
public:
    ExchangeId buyExchange;
    ExchangeId sellExchange;
    double amount;
    double buyPrice;
    double sellPrice;

    Opportunity(ExchangeId buyExchange, ExchangeId sellExchange, double amount, double buyPrice, double sellPrice)
        : buyExchange(buyExchange), sellExchange(sellExchange), amount(amount), buyPrice(buyPrice), sellPrice(sellPrice) {}

    double profit() const
    {
        return (sellPrice - buyPrice) / buyPrice * 100;
    }

private:
    void trace(std::ostream &os) const override
    {
        os << "Opportunity: " << buyExchange << " -> " << sellExchange << " amount: " << amount
            << " (" << buyPrice << " -> " << sellPrice << ") profit: " << std::fixed << std::setprecision(3) << profit() << "%";
    }
};

class StrategyPoplavki : public Strategy
{
public:
    StrategyPoplavki(const std::string &baseAsset,
                     const std::string &quoteAsset,
                     TimersMgr &timers,
                     ExchangeManager &exchangeManager,
                     const std::vector<ExchangeId> &exchangeIds);
    ~StrategyPoplavki() override;

    // Event handling
    void onExchangeUpdate(ExchangeId exchange) override;

    // Scanning
    void scanOpportunities() override;
    void setScanInterval(int ms) override;

    static void timerCallback(int id, void *data);
    void execute() override;

protected:
    void trace(std::ostream &os) const override
    {
        Strategy::trace(os);
    }

private:
    // Exchange data
    struct OrderBookData : public Traceable
    {
        double bestBid = 0.0;
        double bestAsk = 0.0;
        double bestBidQuantity = 0.0;
        double bestAskQuantity = 0.0;
        std::chrono::steady_clock::time_point lastUpdate;

    protected:
        void trace(std::ostream &os) const override
        {
            os << std::fixed << std::setprecision(2)
               << "bid:" << bestBid << "(" << std::setprecision(4) << bestBidQuantity << ")"
               << " ask:" << bestAsk << "(" << std::setprecision(4) << bestAskQuantity << ")";
        }
    };

    // Core components
    std::string baseAsset;
    std::string quoteAsset;
    ExchangeManager &exchangeManager;

    // List of exchanges this strategy uses
    std::vector<ExchangeId> exchangeIds;

    // Order book data
    std::mutex dataMutex;
    std::map<ExchangeId, OrderBookData> orderBookData;
    int timerId = -1; // Store the timer identifier

    // Helper methods
    void updateOrderBookData(ExchangeId exchange, const OrderBook &orderBook);
    Opportunity calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange);

    // For TRACE identification
    const std::vector<ExchangeId> &getExchangeIds() const { return exchangeIds; }

    const ApiExchange &getExchange(ExchangeId exchangeId) const { return *this->exchangeManager.getExchange(exchangeId); }
};