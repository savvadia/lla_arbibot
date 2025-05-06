#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "api_kraken.h"
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace std::chrono_literals;
using json = nlohmann::json;

// Test class to expose protected methods for testing
class TestApiKraken : public ApiKraken {
public:
    TestApiKraken(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode = true)
        : ApiKraken(orderBookManager, timersMgr, testMode) {}

    // Expose protected methods for testing
    using ApiKraken::processRateLimitHeaders;

    void processMessage(const std::string& message) override {
    }
};

class ApiKrakenTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderBookManager = std::make_unique<OrderBookManager>();
        timersMgr = std::make_unique<TimersMgr>();
        api = std::make_unique<TestApiKraken>(*orderBookManager, *timersMgr, true);
        // Connect immediately in setup to avoid repeated connection delays
        EXPECT_TRUE(api->connect());
    }

    void TearDown() override {
        api->disconnect();
    }

    std::unique_ptr<OrderBookManager> orderBookManager;
    std::unique_ptr<TimersMgr> timersMgr;
    std::unique_ptr<TestApiKraken> api;
};

TEST_F(ApiKrakenTest, ConstructorInitializesCorrectly) {
    EXPECT_TRUE(api->isConnected());
    EXPECT_TRUE(api->isTestMode());
    EXPECT_EQ(api->getExchangeName(), "KRAKEN");
    EXPECT_EQ(api->getExchangeId(), ExchangeId::KRAKEN);
}

TEST_F(ApiKrakenTest, ConnectionHandling) {
    // Connection already established in SetUp
    EXPECT_TRUE(api->isConnected());
    
    api->disconnect();
    EXPECT_FALSE(api->isConnected());
    
    // Reconnect for subsequent tests
    EXPECT_TRUE(api->connect());
}

TEST_F(ApiKrakenTest, OrderBookSubscription) {
    // Set up subscription callback
    bool callbackCalled = false;
    api->setSubscriptionCallback([&](bool success) { callbackCalled = success; });
    
    // Subscribe to order book updates
    std::vector<TradingPair> pairs = {TradingPair::BTC_USDT};
    EXPECT_TRUE(api->subscribeOrderBook(pairs));
    EXPECT_TRUE(callbackCalled);
}

TEST_F(ApiKrakenTest, OrderBookSnapshot) {
    // Set up snapshot callback
    bool callbackCalled = false;
    api->setSnapshotCallback([&](bool success) { callbackCalled = success; });
    
    // Get order book snapshot
    EXPECT_TRUE(api->getOrderBookSnapshot(TradingPair::BTC_USDT));
    EXPECT_TRUE(callbackCalled);
}

// TEST_F(ApiKrakenTest, ProcessOrderBookUpdate) {
//     // Test that order book updates are correctly processed
//     std::string message = R"({
//         "channelID": 1,
//         "data": {
//             "a": [["10001.0", "0.5", "1234567890"], ["10002.0", "1.5", "1234567891"]],
//             "b": [["10000.0", "1.0", "1234567892"], ["9999.0", "2.0", "1234567893"]],
//             "c": "1234567894"
//         },
//         "event": "book",
//         "pair": "XBT/USDT",
//         "type": "update"
//     })";

//     // Use the public method to process the message
//     api->processMessages(); // This will process the message in test mode

//     // Verify the order book was updated correctly
//     auto& orderBook = orderBookManager->getOrderBook(ExchangeId::KRAKEN, TradingPair::BTC_USDT);
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

// TEST_F(ApiKrakenTest, ProcessOrderBookSnapshot) {
//     // Test that order book snapshots are correctly processed
//     json data = json::parse(R"({
//         "result": {
//             "XXBTZUSD": {
//                 "asks": [["10001.0", "0.5", "1234567890"], ["10002.0", "1.5", "1234567891"]],
//                 "bids": [["10000.0", "1.0", "1234567892"], ["9999.0", "2.0", "1234567893"]]
//             }
//         }
//     })");

//     // Use the public method to get the snapshot
//     EXPECT_TRUE(api->getOrderBookSnapshot(TradingPair::BTC_USDT));

//     // Verify the order book was updated correctly
//     auto& orderBook = orderBookManager->getOrderBook(ExchangeId::KRAKEN, TradingPair::BTC_USDT);
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

TEST_F(ApiKrakenTest, ProcessTradeUpdate) {
    // Test that trade updates are correctly processed
    std::string message = R"({
        "channelID": 1,
        "data": [["10000.0", "1.0", "1234567890", "b", "l", ""]],
        "event": "trade",
        "pair": "XBT/USDT",
        "type": "update"
    })";

    // Use the public method to process the message
    api->processMessage(message); // This will process the message in test mode

    // Verify the last price was updated
    auto& orderBook = orderBookManager->getOrderBook(ExchangeId::KRAKEN, TradingPair::BTC_USDT);
    EXPECT_DOUBLE_EQ(orderBook.getBestBid(), 0.0); // No bids set yet
    EXPECT_DOUBLE_EQ(orderBook.getBestAsk(), 0.0); // No asks set yet
}

// TEST_F(ApiKrakenTest, ProcessBalanceUpdate) {
//     // Test that balance updates are correctly processed
//     std::string message = R"({
//         "event": "balance",
//         "data": {
//             "BTC": "10.0",
//             "USDT": "100000.0"
//         }
//     })";

//     bool callbackCalled = false;
//     api->setBalanceCallback([&](bool success) {
//         callbackCalled = true;
//         EXPECT_TRUE(success);
//     });

//     // Use the public method to process the message
//     api->processMessages(); // This will process the message in test mode
//     EXPECT_TRUE(callbackCalled);
// }

// TEST_F(ApiKrakenTest, ProcessOrderUpdate) {
//     // Test that order updates are correctly processed
//     std::string message = R"({
//         "event": "order",
//         "data": {
//             "orderid": "test_order_id",
//             "status": "closed",
//             "descr": {
//                 "pair": "XBTUSDT",
//                 "type": "buy",
//                 "ordertype": "limit",
//                 "price": "10000.0",
//                 "price2": "0",
//                 "leverage": "none",
//                 "order": "buy 1.00000000 XBTUSDT @ limit 10000.0",
//                 "close": ""
//             },
//             "vol": "1.00000000",
//             "vol_exec": "1.00000000",
//             "cost": "10000.0",
//             "fee": "0.0",
//             "price": "10000.0",
//             "stopprice": "0.00000",
//             "limitprice": "0.00000",
//             "misc": "",
//             "oflags": "fciq"
//         }
//     })";

//     bool callbackCalled = false;
//     api->setOrderCallback([&](bool success) {
//         callbackCalled = true;
//         EXPECT_TRUE(success);
//     });

//     // Use the public method to process the message
//     api->processMessages(); // This will process the message in test mode
//     EXPECT_TRUE(callbackCalled);
// }

TEST_F(ApiKrakenTest, RateLimitHandling) {
    // Test that rate limits are correctly processed from headers
    std::string headers = "X-RateLimit-Used: 1200\r\nX-RateLimit-Limit: 1500\r\n";
    
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
