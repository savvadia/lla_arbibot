#pragma once

#include <string>
#include <mutex>
#include <vector>
#include "strategy.h"

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