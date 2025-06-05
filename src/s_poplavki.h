#pragma once

#include <string>
#include "balance.h"
#include <mutex>
#include <vector>
#include <chrono>
#include "tracer.h"

class Opportunity : public Traceable
{
public:
    ExchangeId buyExchange;
    ExchangeId sellExchange;
    TradingPair pair;
    std::chrono::system_clock::time_point timestamp;
    double amount;
    double buyPrice;
    double sellPrice;

    Opportunity(ExchangeId buyExchange, ExchangeId sellExchange, TradingPair pair,
        double amount, double buyPrice, double sellPrice, std::chrono::system_clock::time_point timestamp)
        : buyExchange(buyExchange), sellExchange(sellExchange), pair(pair), timestamp(timestamp),
        amount(amount), buyPrice(buyPrice), sellPrice(sellPrice) {}

    double profit() const
    {
        return (sellPrice - buyPrice) / buyPrice * 100;
    }

private:
    void trace(std::ostream &os) const override
    {
        os << "Opportunity: " << buyExchange << " -> " << sellExchange << " amount: " << amount
            << " (" << buyPrice << " -> " << sellPrice << ") profit: " << profit() << "%";
    }
};

class Strategy : public Traceable
{
protected:
    BalanceData balances;

public:
    Strategy(std::string name, std::string coin, std::string stableCoin, TradingPair pair);

    std::string name;
    std::string coin;       // coin to trade
    std::string stableCoin; // coin to trade against
    TradingPair pair;
    Opportunity bestOpportunity1;
    Opportunity bestOpportunity2;

    void setBalances(BalanceData balances);

    // Core strategy methods
    virtual void execute() = 0;

    // Optional methods for exchange updates
    virtual void onExchangeUpdate(ExchangeId exchange) {}

    // Optional methods for periodic scanning
    virtual void scanOpportunities() {}
    virtual void startTimerToScan(int ms) {}
    static void resetBestSeenOpportunityTimerCallback(int id, void *data);

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

class StrategyPoplavki : public Strategy
{
public:
    StrategyPoplavki(const std::string &baseAsset,
                     const std::string &quoteAsset,
                     TradingPair pair,
                     const std::vector<ExchangeId> &exchangeIds);
    ~StrategyPoplavki() override;

    // Event handling
    void onExchangeUpdate(ExchangeId exchange) override;

    // Scanning
    void scanOpportunities() override;
    void startTimerToScan(int ms) override;

    static void timerCallback(int id, void *data);
    void execute() override;

protected:
    void trace(std::ostream &os) const override
    {
        Strategy::trace(os);
    }

private:
    // Exchange data
    std::string baseAsset;
    std::string quoteAsset;

    // List of exchanges this strategy uses
    std::vector<ExchangeId> exchangeIds;

    // Order book data
    std::mutex dataMutex;
    int timerId = -1; // Store the timer identifier

    // Helper methods
    void updateOrderBookData(ExchangeId exchange);
    Opportunity calculateProfit(ExchangeId buyExchange, ExchangeId sellExchange, TradingPair pair);

    // For TRACE identification
    const std::vector<ExchangeId> &getExchangeIds() const { return exchangeIds; }
};