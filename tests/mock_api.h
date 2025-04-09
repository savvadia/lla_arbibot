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
    bool subscribeOrderBook(TradingPair pair) override;

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
    bool isConnected() const override { return m_connected; }

    // Callback setters
    void setSubscriptionCallback(std::function<void(bool)> callback) override;
    void setSnapshotCallback(std::function<void(bool)> callback) override;
    void setOrderCallback(std::function<void(bool)> callback) override;
    void setBalanceCallback(std::function<void(bool)> callback) override;

    // Test helper methods
    void simulateOrderBookUpdate(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);

protected:

private:
    // Internal symbol conversion methods
    TradingPair symbolToTradingPair(const std::string& symbol) const;
    std::string tradingPairToSymbol(TradingPair pair);

    // Mock WebSocket event handlers
    void handleMockMessage(const std::string& msg);

    std::string m_name;
    ExchangeId m_id;
    std::map<TradingPair, std::string> m_symbolMap;
    bool m_connected{false};
    
    // Callbacks
    std::function<void(bool)> m_subscriptionCallback;
    std::function<void()> m_updateCallback;
    std::function<void(bool)> m_snapshotCallback;
    std::function<void(bool)> m_orderCallback;
    std::function<void(bool)> m_balanceCallback;
    
    // Mock WebSocket state
    bool m_subscribed{false};
}; 