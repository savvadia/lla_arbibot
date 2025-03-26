#include <gtest/gtest.h>
#include "mock_api.h"
#include "../src/orderbook.h"
#include <thread>
#include <chrono>

class ApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_orderBookManager = std::make_unique<OrderBookManager>();
        m_binanceApi = std::make_unique<MockApi>(*m_orderBookManager, "Binance");
        m_krakenApi = std::make_unique<MockApi>(*m_orderBookManager, "Kraken");
    }

    void TearDown() override {
        m_binanceApi.reset();
        m_krakenApi.reset();
        m_orderBookManager.reset();
    }

    std::unique_ptr<OrderBookManager> m_orderBookManager;
    std::unique_ptr<MockApi> m_binanceApi;
    std::unique_ptr<MockApi> m_krakenApi;
};

TEST_F(ApiTest, ExchangeIdentifiers) {
    EXPECT_EQ(m_binanceApi->getExchangeId(), ExchangeId::BINANCE);
    EXPECT_EQ(m_krakenApi->getExchangeId(), ExchangeId::KRAKEN);
}

TEST_F(ApiTest, TradingPairConversion) {
    // Test Binance symbol conversion
    EXPECT_EQ(m_binanceApi->symbolToTradingPair("BTCUSDT"), TradingPair::BTC_USDT);
    EXPECT_EQ(m_binanceApi->symbolToTradingPair("ETHUSDT"), TradingPair::ETH_USDT);
    EXPECT_EQ(m_binanceApi->symbolToTradingPair("XTZUSDT"), TradingPair::XTZ_USDT);
    EXPECT_EQ(m_binanceApi->symbolToTradingPair("UNKNOWN"), TradingPair::UNKNOWN);

    // Test Kraken symbol conversion
    EXPECT_EQ(m_krakenApi->symbolToTradingPair("XBT/USD"), TradingPair::BTC_USDT);
    EXPECT_EQ(m_krakenApi->symbolToTradingPair("ETH/USD"), TradingPair::ETH_USDT);
    EXPECT_EQ(m_krakenApi->symbolToTradingPair("XTZ/USD"), TradingPair::XTZ_USDT);
    EXPECT_EQ(m_krakenApi->symbolToTradingPair("UNKNOWN"), TradingPair::UNKNOWN);

    // Test reverse conversion
    EXPECT_EQ(m_binanceApi->tradingPairToSymbol(TradingPair::BTC_USDT), "BTCUSDT");
    EXPECT_EQ(m_krakenApi->tradingPairToSymbol(TradingPair::BTC_USDT), "XBT/USD");
}

TEST_F(ApiTest, OrderBookSubscription) {
    bool binanceSubscribed = false;
    bool krakenSubscribed = false;

    m_binanceApi->setSubscriptionCallback([&binanceSubscribed](bool success) {
        binanceSubscribed = success;
    });

    m_krakenApi->setSubscriptionCallback([&krakenSubscribed](bool success) {
        krakenSubscribed = success;
    });

    // Connect to exchanges
    ASSERT_TRUE(m_binanceApi->connect());
    ASSERT_TRUE(m_krakenApi->connect());

    // Subscribe to order books
    ASSERT_TRUE(m_binanceApi->subscribeOrderBook(TradingPair::BTC_USDT));
    ASSERT_TRUE(m_krakenApi->subscribeOrderBook(TradingPair::BTC_USDT));

    // Wait for subscription callbacks with timeout
    const auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(50); // 50ms timeout

    while (!binanceSubscribed || !krakenSubscribed) {
        if (std::chrono::steady_clock::now() - startTime > timeout) {
            FAIL() << "Subscription timeout: Binance=" << binanceSubscribed 
                  << ", Kraken=" << krakenSubscribed;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(binanceSubscribed);
    EXPECT_TRUE(krakenSubscribed);
}

TEST_F(ApiTest, OrderBookUpdates) {
    bool updateReceived = false;
    m_binanceApi->setUpdateCallback([&updateReceived]() {
        updateReceived = true;
    });

    // Connect and subscribe
    ASSERT_TRUE(m_binanceApi->connect());
    ASSERT_TRUE(m_binanceApi->subscribeOrderBook(TradingPair::BTC_USDT));

    // Get initial snapshot
    ASSERT_TRUE(m_binanceApi->getOrderBookSnapshot(TradingPair::BTC_USDT));

    // Send an update
    std::vector<PriceLevel> bids = {
        {50000.0, 1.0},
        {49999.0, 2.0}
    };
    std::vector<PriceLevel> asks = {
        {50001.0, 1.0},
        {50002.0, 2.0}
    };
    m_binanceApi->sendOrderBookUpdate(bids, asks);

    // Wait for update with timeout
    const auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(50); // 50ms timeout

    while (!updateReceived) {
        if (std::chrono::steady_clock::now() - startTime > timeout) {
            FAIL() << "Update timeout: no update received";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(updateReceived);

    // Verify order book state
    const auto& orderBook = m_orderBookManager->getOrderBook(TradingPair::BTC_USDT);
    EXPECT_EQ(orderBook.getBids().size(), 2);
    EXPECT_EQ(orderBook.getAsks().size(), 2);
    EXPECT_EQ(orderBook.getBids()[0].price, 50000.0);
    EXPECT_EQ(orderBook.getAsks()[0].price, 50001.0);
}

TEST_F(ApiTest, OrderBookSnapshot) {
    bool snapshotReceived = false;
    m_binanceApi->setSnapshotCallback([&snapshotReceived](bool success) {
        snapshotReceived = success;
    });

    // Connect and get snapshot
    ASSERT_TRUE(m_binanceApi->connect());
    ASSERT_TRUE(m_binanceApi->getOrderBookSnapshot(TradingPair::BTC_USDT));

    // Wait for snapshot with timeout
    const auto startTime = std::chrono::steady_clock::now();
    const auto timeout = std::chrono::milliseconds(50); // 50ms timeout

    while (!snapshotReceived) {
        if (std::chrono::steady_clock::now() - startTime > timeout) {
            FAIL() << "Snapshot timeout: no snapshot received";
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    EXPECT_TRUE(snapshotReceived);

    // Verify order book state
    const auto& orderBook = m_orderBookManager->getOrderBook(TradingPair::BTC_USDT);
    EXPECT_EQ(orderBook.getBids().size(), 3);
    EXPECT_EQ(orderBook.getAsks().size(), 3);
    EXPECT_EQ(orderBook.getBids()[0].price, 50000.0);
    EXPECT_EQ(orderBook.getAsks()[0].price, 50001.0);
}

TEST_F(ApiTest, ConnectionManagement) {
    // Test connection
    ASSERT_TRUE(m_binanceApi->connect());
    ASSERT_TRUE(m_krakenApi->connect());

    // Test disconnection
    m_binanceApi->disconnect();
    m_krakenApi->disconnect();

    // Verify operations fail when disconnected
    EXPECT_FALSE(m_binanceApi->subscribeOrderBook(TradingPair::BTC_USDT));
    EXPECT_FALSE(m_krakenApi->subscribeOrderBook(TradingPair::BTC_USDT));
    EXPECT_FALSE(m_binanceApi->getOrderBookSnapshot(TradingPair::BTC_USDT));
    EXPECT_FALSE(m_krakenApi->getOrderBookSnapshot(TradingPair::BTC_USDT));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 