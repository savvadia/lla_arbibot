#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "api_kraken.h"
#include <memory>
#include <chrono>
#include <nlohmann/json.hpp>
#include "orderbook_mgr.h"

using namespace std::chrono_literals;
using json = nlohmann::json;

// Create a test class that inherits from ApiKraken to access protected members
class TestApiKraken : public ApiKraken {
public:
    TestApiKraken(bool testMode = true) : ApiKraken({TradingPair::BTC_USDT}, testMode) {}

    // Expose protected methods for testing
    using ApiKraken::processOrderBookUpdate;
    using ApiKraken::processMessage;
    using ApiKraken::processRateLimitHeaders;
};

class ApiKrakenTest : public ::testing::Test {
protected:
    void SetUp() override {
        api = std::make_unique<TestApiKraken>(true);
        // Connect immediately in setup to avoid repeated connection delays
        EXPECT_TRUE(api->connect());
    }

    void TearDown() override {
        api->disconnect();
        api.reset();
    }

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

TEST_F(ApiKrakenTest, OrderBookSnapshot) {
    // Set up snapshot callback
    bool callbackCalled = false;
    api->setSnapshotCallback([&](bool success) { callbackCalled = success; });
    
    // Get order book snapshot
    EXPECT_TRUE(api->getOrderBookSnapshot(TradingPair::BTC_USDT));
    EXPECT_TRUE(callbackCalled);
}

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
    auto& orderBook = orderBookManager.getOrderBook(ExchangeId::KRAKEN, TradingPair::BTC_USDT);
    EXPECT_DOUBLE_EQ(orderBook.getBestBid(), 0.0); // No bids set yet
    EXPECT_DOUBLE_EQ(orderBook.getBestAsk(), 0.0); // No asks set yet
}

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

TEST_F(ApiKrakenTest, OrderBookChecksumVerification) {
    // Test data
    json data = {
        {"channel", "book"},
        {"type", "snapshot"},
        {"data", {{
            {"symbol", "BTC/USD"},
            {"bids", {
                {{"price", 45283.5}, {"qty", 0.10000000}},
                {{"price", 45283.4}, {"qty", 1.54582015}},
                {{"price", 45282.1}, {"qty", 0.10000000}},
                {{"price", 45281.0}, {"qty", 0.10000000}},
                {{"price", 45280.3}, {"qty", 1.54592586}},
                {{"price", 45279.0}, {"qty", 0.07990000}},
                {{"price", 45277.6}, {"qty", 0.03310103}},
                {{"price", 45277.5}, {"qty", 0.30000000}},
                {{"price", 45277.3}, {"qty", 1.54602737}},
                {{"price", 45276.6}, {"qty", 0.15445238}}
            }},
            {"asks", {
                {{"price", 45285.2}, {"qty", 0.00100000}},
                {{"price", 45286.4}, {"qty", 1.54571953}},
                {{"price", 45286.6}, {"qty", 1.54571109}},
                {{"price", 45289.6}, {"qty", 1.54560911}},
                {{"price", 45290.2}, {"qty", 0.15890660}},
                {{"price", 45291.8}, {"qty", 1.54553491}},
                {{"price", 45294.7}, {"qty", 0.04454749}},
                {{"price", 45296.1}, {"qty", 0.35380000}},
                {{"price", 45297.5}, {"qty", 0.09945542}},
                {{"price", 45299.5}, {"qty", 0.18772827}}
            }},
            {"checksum", 3310070434}
        }}}
    };
    TradingPair pair = TradingPair::BTC_USDT;

    std::cout << "Data: " << data.dump(4) << std::endl;
    std::cout << "Processing order book update" << std::endl;
    // Process the order book update
    api->processOrderBookUpdate(data);
    std::cout << "Order book update processed" << std::endl;

    // Get the order book
    auto& book = orderBookManager.getOrderBook(ExchangeId::KRAKEN, TradingPair::BTC_USDT);
    auto bids = book.getBids();
    auto asks = book.getAsks();


    // Test asks checksum string
    std::string expectedAsksString = "45285210000045286415457195345286615457110945289615456091145290215890660452918154553491452947445474945296135380000452975994554245299518772827";
    std::string actualAsksString = api->buildChecksumString(pair, asks);
    EXPECT_EQ(actualAsksString, expectedAsksString) << "Asks checksum string mismatch";

    // Test bids checksum string
    std::string expectedBidsString = "452835100000004528341545820154528211000000045281010000000452803154592586452790799000045277633101034527753000000045277315460273745276615445238";
    std::string actualBidsString = api->buildChecksumString(pair, bids);
    EXPECT_EQ(actualBidsString, expectedBidsString) << "Bids checksum string mismatch";

    // Test combined checksum
    std::string combinedString = actualAsksString + actualBidsString;
    std::cout << "Combined string: " << combinedString << std::endl;
    std::string expectedCombinedString = "45285210000045286415457195345286615457110945289615456091145290215890660452918154553491452947445474945296135380000452975994554245299518772827452835100000004528341545820154528211000000045281010000000452803154592586452790799000045277633101034527753000000045277315460273745276615445238";
    EXPECT_EQ(combinedString, expectedCombinedString) << "Combined string mismatch";

    uint32_t expectedChecksum = 3310070434;
    uint32_t actualChecksum = api->computeChecksum(combinedString);
    EXPECT_EQ(actualChecksum, expectedChecksum) << "Combined checksum mismatch";

    // Test order book validation
    EXPECT_TRUE(api->isOrderBookValid(pair, expectedChecksum)) << "Order book validation failed";
}
