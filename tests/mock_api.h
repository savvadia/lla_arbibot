#pragma once

#include "../src/api_exchange.h"
#include "../src/orderbook.h"
#include "../src/timers.h"
#include <string>
#include <functional>
#include <map>

using json = nlohmann::json;

class MockApi : public ApiExchange {
public:
    MockApi(OrderBookManager& orderBookManager, TimersMgr& timersMgr, const std::string& name, bool testMode = true);
    ~MockApi() override;

    // Initialize WebSocket connection
    bool connect() override;
    void disconnect() override;

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook(std::vector<TradingPair> pairs) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;

    // Process incoming messages
    void processMessages() override;

    // Order management
    bool placeOrder(TradingPair pair, OrderType type, double price, double amount) override;
    bool cancelOrder(const std::string& orderId) override;
    bool getBalance(const std::string& currency) override;

    std::string getExchangeName() const override { return m_name; }
    ExchangeId getExchangeId() const override { return m_id; }

    // Test helper methods
    void simulateOrderBookUpdate(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);

protected:
    // Override the cooldown method for mock-specific rate limiting
    void processRateLimitHeaders(const std::string& headers) override;

private:
    // Internal symbol conversion methods
    TradingPair symbolToTradingPair(const std::string& symbol) const;
    std::string tradingPairToSymbol(TradingPair pair);

    // Mock WebSocket event handlers
    void handleMockMessage(const std::string& msg);

    std::string m_name;
    ExchangeId m_id;
    
    // Callbacks
    std::function<void(bool)> m_subscriptionCallback;
    std::function<void()> m_updateCallback;
    std::function<void(bool)> m_snapshotCallback;
    std::function<void(bool)> m_orderCallback;
    std::function<void(bool)> m_balanceCallback;
}; 