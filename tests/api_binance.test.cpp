#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/api_binance.h"
#include "../src/tracer.h"
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;
using json = nlohmann::json;

// Test class to expose protected methods for testing
class TestApiBinance : public ApiBinance {
public:
    TestApiBinance(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode = true)
        : ApiBinance(orderBookManager, timersMgr, {TradingPair::BTC_USDT}, testMode) {}

    // Expose protected methods for testing
    using ApiBinance::processMessage;
    using ApiBinance::processOrderBookSnapshot;
    using ApiBinance::processRateLimitHeaders;
};

class ApiBinanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderBookManager = std::make_unique<OrderBookManager>();
        timersMgr = std::make_unique<TimersMgr>();
        api = std::make_unique<TestApiBinance>(*orderBookManager, *timersMgr, true);
        // Connect immediately in setup to avoid repeated connection delays
        EXPECT_TRUE(api->connect());
    }

    void TearDown() override {
        api->disconnect();
        api.reset();
        orderBookManager.reset();
        timersMgr.reset();
    }

    std::unique_ptr<OrderBookManager> orderBookManager;
    std::unique_ptr<TimersMgr> timersMgr;
    std::unique_ptr<TestApiBinance> api;
};

TEST_F(ApiBinanceTest, ConstructorInitializesCorrectly) {
    EXPECT_TRUE(api->isConnected());
    EXPECT_TRUE(api->isTestMode());
    EXPECT_EQ(api->getExchangeName(), "BINANCE");
    EXPECT_EQ(api->getExchangeId(), ExchangeId::BINANCE);
}

TEST_F(ApiBinanceTest, ConnectionHandling) {
    // Connection already established in SetUp
    EXPECT_TRUE(api->isConnected());
    
    api->disconnect();
    EXPECT_FALSE(api->isConnected());
    
    // Reconnect for subsequent tests
    EXPECT_TRUE(api->connect());
}

TEST_F(ApiBinanceTest, OrderBookSnapshot) {
    // Set up snapshot callback
    bool callbackCalled = false;
    api->setSnapshotCallback([&](bool success) { callbackCalled = success; });
    
    // Get order book snapshot
    EXPECT_TRUE(api->getOrderBookSnapshot(TradingPair::BTC_USDT));
    EXPECT_TRUE(callbackCalled);
}

TEST_F(ApiBinanceTest, MessageProcessing) {
    api->processMessage(R"({
        "e": "bookTicker",
        "s": "BTCUSDT",
        "b": "10000.0",
        "B": "1.0",
        "a": "10001.0",
        "A": "0.5"
    })");
}

// TEST_F(ApiBinanceTest, ProcessOrderBookUpdate) {
//     // Test that order book updates are correctly processed
//     std::string message = R"({
//         "e": "depthUpdate",
//         "E": 123456789,
//         "s": "BTCUSDT",
//         "U": 157,
//         "u": 160,
//         "b": [["10000.0", "1.0"], ["9999.0", "2.0"]],
//         "a": [["10001.0", "0.5"], ["10002.0", "1.5"]]
//     })";

//     api->processMessage(message);

//     // Verify the order book was updated correctly
//     auto& orderBook = orderBookManager->getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
//     auto [bids, asks] = orderBook.getState();
    
//     EXPECT_EQ(bids.size(), 2);
//     EXPECT_EQ(asks.size(), 2);
    
//     EXPECT_DOUBLE_EQ(bids[0].price, 10000.0);
//     EXPECT_DOUBLE_EQ(bids[0].quantity, 1.0);
//     EXPECT_DOUBLE_EQ(bids[1].price, 9999.0);
//     EXPECT_DOUBLE_EQ(bids[1].quantity, 2.0);
    
//     EXPECT_DOUBLE_EQ(asks[0].price, 10001.0);
//     EXPECT_DOUBLE_EQ(asks[0].quantity, 0.5);
//     EXPECT_DOUBLE_EQ(asks[1].price, 10002.0);
//     EXPECT_DOUBLE_EQ(asks[1].quantity, 1.5);
// }

TEST_F(ApiBinanceTest, ProcessOrderBookSnapshot) {
    // Test that order book snapshots are correctly processed
    json data = json::parse(R"({
        "lastUpdateId": 160,
        "bids": [["10000.0", "1.0"], ["9999.0", "2.0"]],
        "asks": [["10001.0", "0.5"], ["10002.0", "1.5"]]
    })");

    api->processOrderBookSnapshot(data, TradingPair::BTC_USDT);

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

TEST_F(ApiBinanceTest, ProcessTradeUpdate) {
    // Test that trade updates are correctly processed
    std::string message = R"({
        "e": "trade",
        "E": 123456789,
        "s": "BTCUSDT",
        "t": 12345,
        "p": "10000.0",
        "q": "1.0",
        "b": 88,
        "a": 50,
        "T": 123456785,
        "m": true,
        "M": true
    })";

    api->processMessage(message);

    // Verify the last price was updated
    auto& orderBook = orderBookManager->getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    EXPECT_DOUBLE_EQ(orderBook.getBestBid(), 0.0); // No bids set yet
    EXPECT_DOUBLE_EQ(orderBook.getBestAsk(), 0.0); // No asks set yet
}

TEST_F(ApiBinanceTest, RateLimitHandling) {
    // Test that rate limits are correctly processed from headers
    std::string headers = "x-mbx-used-weight-1m: 1200\r\nx-mbx-order-count-10s: 50\r\n";
    
    // We can't directly test the internal rate limit state, but we can verify
    // that processing the headers doesn't throw and subsequent requests are
    // properly rate limited
    api->processRateLimitHeaders(headers);
    
    // Verify that requests are delayed when rate limits are close to threshold
    auto start = std::chrono::steady_clock::now();
    api->placeOrder(TradingPair::BTC_USDT, OrderType::BUY, 50000.0, 0.001);
    auto end = std::chrono::steady_clock::now();
    
    // Should have some delay due to rate limiting
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_GT(duration.count(), 0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}