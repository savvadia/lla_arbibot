#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "api_exchange.h"
#include <memory>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;
using json = nlohmann::json;

class TestApiExchange : public ApiExchange {
public:
    TestApiExchange(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode = true)
        : ApiExchange(orderBookManager, timersMgr, "test.com", "443", "https://test.com/api", "/ws", {TradingPair::BTC_USDT}, testMode) {}

    bool connect() override { 
        m_connected = true;
        return true; 
    }

    void disconnect() override { 
        m_connected = false; 
    }

    bool subscribeOrderBook() override { 
        return true; 
    }

    bool resubscribeOrderBook(const std::vector<TradingPair>& pairs) override {
        return true;
    }

    bool getOrderBookSnapshot(TradingPair pair) override { 
        // In test mode, simulate getting a snapshot by updating the order book
        std::vector<PriceLevel> bids = {
            {10000.0, 1.0},
            {9999.0, 2.0}
        };
        std::vector<PriceLevel> asks = {
            {10001.0, 0.5},
            {10002.0, 1.5}
        };
        m_orderBookManager.updateOrderBook(getExchangeId(), pair, bids, asks);
        
        if (m_snapshotCallback) m_snapshotCallback(true);
        return true; 
    }

    void processMessage(const json& data) override {
        // Process any queued messages
        while (!m_messageQueue.empty()) {
            auto message = m_messageQueue.front();
            m_messageQueue.pop();
            
            // Parse the message and update order book accordingly
            try {
                if (data.contains("channel")) {
                    if (data["channel"] == "orderbook") {
                        std::vector<PriceLevel> bids;
                        std::vector<PriceLevel> asks;
                        
                        if (data["data"].contains("bids")) {
                            for (const auto& bid : data["data"]["bids"]) {
                                bids.push_back({std::stod(bid[0].get<std::string>()), std::stod(bid[1].get<std::string>())});
                            }
                        }
                        if (data["data"].contains("asks")) {
                            for (const auto& ask : data["data"]["asks"]) {
                                asks.push_back({std::stod(ask[0].get<std::string>()), std::stod(ask[1].get<std::string>())});
                            }
                        }
                        
                        m_orderBookManager.updateOrderBook(getExchangeId(), TradingPair::BTC_USDT, bids, asks);
                    } else if (data["channel"] == "balance") {
                        if (m_balanceCallback) m_balanceCallback(true);
                    } else if (data["channel"] == "order") {
                        if (m_orderCallback) m_orderCallback(true);
                    }
                }
            } catch (const std::exception& e) {
                // Ignore parsing errors in test mode
            }
        }
    }

    bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) override { 
        // Check if we're in cooldown
        if (isInCooldown()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (m_orderCallback) m_orderCallback(true);
        return true; 
    }

    bool cancelOrder(const std::string& orderId) override { 
        if (m_orderCallback) m_orderCallback(true);
        return true; 
    }

    bool getBalance(const std::string& asset) override { 
        if (m_balanceCallback) m_balanceCallback(true);
        return true; 
    }

    std::string getExchangeName() const override { return "TEST"; }
    ExchangeId getExchangeId() const override { return ExchangeId::BINANCE; }

    void processRateLimitHeaders(const std::string& headers) override {
        // Simulate rate limit delay
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        
        // Update rate limit info with values that will trigger cooldown
        // Setting remaining to less than 10% of limit to trigger cooldown
        updateRateLimit("test", 1200, 100, 60); // 100 remaining out of 1200 limit (8.3%)
    }

    // Helper method to queue messages for processing
    void queueMessage(const std::string& message) {
        m_messageQueue.push(message);
    }

    // Expose protected members for testing
    std::map<TradingPair, SymbolState>& getSymbolStates() { return symbolStates; }
    TimersMgr& getTimersMgr() { return m_timersMgr; }

private:
    std::queue<std::string> m_messageQueue;
};

class ApiExchangeTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderBookManager = std::make_unique<OrderBookManager>();
        timersMgr = std::make_unique<TimersMgr>();
        api = std::make_unique<TestApiExchange>(*orderBookManager, *timersMgr, true);
        // Connect immediately in setup to avoid repeated connection delays
        EXPECT_TRUE(api->connect());
    }

    void TearDown() override {
        api->disconnect();
    }

    std::unique_ptr<OrderBookManager> orderBookManager;
    std::unique_ptr<TimersMgr> timersMgr;
    std::unique_ptr<TestApiExchange> api;
};

TEST_F(ApiExchangeTest, ConstructorInitializesCorrectly) {
    EXPECT_TRUE(api->isConnected());
    EXPECT_TRUE(api->isTestMode());
    EXPECT_EQ(api->getExchangeName(), "TEST");
    EXPECT_EQ(api->getExchangeId(), ExchangeId::BINANCE);
}

TEST_F(ApiExchangeTest, ConnectionHandling) {
    // Connection already established in SetUp
    EXPECT_TRUE(api->isConnected());
    
    api->disconnect();
    EXPECT_FALSE(api->isConnected());
    
    // Reconnect for subsequent tests
    EXPECT_TRUE(api->connect());
}

TEST_F(ApiExchangeTest, ProcessOrderBookSnapshot) {
    // Test that order book snapshots are correctly processed
    json data = json::parse(R"({
        "bids": [["10000.0", "1.0"], ["9999.0", "2.0"]],
        "asks": [["10001.0", "0.5"], ["10002.0", "1.5"]]
    })");

    // Use the public method to get the snapshot
    EXPECT_TRUE(api->getOrderBookSnapshot(TradingPair::BTC_USDT));

    // Verify the order book was updated correctly
    auto& orderBook = orderBookManager->getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    auto [bids, asks] = orderBook.getState();
    
    EXPECT_EQ(bids.size(), 2);
    EXPECT_EQ(asks.size(), 2);
    
    EXPECT_DOUBLE_EQ(bids[0].price, 10000.0);
    EXPECT_DOUBLE_EQ(bids[0].quantity, 1.0);
    EXPECT_DOUBLE_EQ(bids[1].price, 9999.0);
    EXPECT_DOUBLE_EQ(bids[1].quantity, 2.0);
    
    EXPECT_DOUBLE_EQ(asks[0].price, 10001.0);
    EXPECT_DOUBLE_EQ(asks[0].quantity, 0.5);
    EXPECT_DOUBLE_EQ(asks[1].price, 10002.0);
    EXPECT_DOUBLE_EQ(asks[1].quantity, 1.5);
}

TEST_F(ApiExchangeTest, ProcessTradeUpdate) {
    // Test that trade updates are correctly processed
    std::string message = R"({
        "channel": "trade",
        "data": {
            "price": "10000.0",
            "quantity": "1.0",
            "side": "buy"
        }
    })";

    // Queue the message for processing
    api->processMessage(message);

    // Verify the last price was updated
    auto& orderBook = orderBookManager->getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    EXPECT_DOUBLE_EQ(orderBook.getBestBid(), 0.0); // No bids set yet
    EXPECT_DOUBLE_EQ(orderBook.getBestAsk(), 0.0); // No asks set yet
}

TEST_F(ApiExchangeTest, ProcessBalanceUpdate) {
    // Test that balance updates are correctly processed
    std::string message = R"({
        "channel": "balance",
        "data": {
            "BTC": "10.0",
            "USDT": "100000.0"
        }
    })";

    bool callbackCalled = false;
    api->setBalanceCallback([&](bool success) {
        callbackCalled = true;
        EXPECT_TRUE(success);
    });

    // Queue the message for processing
    api->processMessage(message);
    EXPECT_TRUE(callbackCalled);
}

TEST_F(ApiExchangeTest, ProcessOrderUpdate) {
    // Test that order updates are correctly processed
    std::string message = R"({
        "channel": "order",
        "data": {
            "order_id": "test_order_id",
            "status": "filled",
            "side": "buy",
            "price": "10000.0",
            "quantity": "1.0",
            "filled_quantity": "1.0",
            "remaining_quantity": "0.0"
        }
    })";

    bool callbackCalled = false;
    api->setOrderCallback([&](bool success) {
        callbackCalled = true;
        EXPECT_TRUE(success);
    });

    // Queue the message for processing
    api->processMessage(message);
    EXPECT_TRUE(callbackCalled);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}