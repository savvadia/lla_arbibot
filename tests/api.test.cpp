#include <gtest/gtest.h>
#include "../src/api_binance.h"
#include "../src/api_kraken.h"
#include "../src/orderbook.h"
#include "../src/types.h"
#include <memory>
#include <thread>
#include <chrono>
#include <atomic>
#include <iostream>

class ApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderBookManager = std::make_unique<OrderBookManager>();
        binanceApi = std::make_unique<BinanceApi>(*orderBookManager);
        krakenApi = std::make_unique<KrakenApi>(*orderBookManager);
    }

    void TearDown() override {
        binanceApi.reset();
        krakenApi.reset();
        orderBookManager.reset();
    }

    std::unique_ptr<OrderBookManager> orderBookManager;
    std::unique_ptr<BinanceApi> binanceApi;
    std::unique_ptr<KrakenApi> krakenApi;
};

// Test exchange names and IDs
TEST_F(ApiTest, ExchangeIdentifiers) {
    EXPECT_EQ(binanceApi->getExchangeName(), "Binance");
    EXPECT_EQ(binanceApi->getExchangeId(), ExchangeId::BINANCE);
    EXPECT_EQ(krakenApi->getExchangeName(), "Kraken");
    EXPECT_EQ(krakenApi->getExchangeId(), ExchangeId::KRAKEN);
}

// Test trading pair conversion
TEST_F(ApiTest, TradingPairConversion) {
    TradingPair btcUsdt = TradingPair::BTC_USDT;
    EXPECT_EQ(binanceApi->tradingPairToSymbol(btcUsdt), "BTCUSDT");
    EXPECT_EQ(krakenApi->tradingPairToSymbol(btcUsdt), "XBT/USD");
}

// Test order book subscription
TEST_F(ApiTest, OrderBookSubscription) {
    bool binanceSubscribed = false;
    bool krakenSubscribed = false;

    binanceApi->setSubscriptionCallback([&binanceSubscribed](bool success) {
        binanceSubscribed = success;
    });

    krakenApi->setSubscriptionCallback([&krakenSubscribed](bool success) {
        krakenSubscribed = success;
    });

    TradingPair btcUsdt = TradingPair::BTC_USDT;
    EXPECT_TRUE(binanceApi->subscribeOrderBook(btcUsdt));
    EXPECT_TRUE(krakenApi->subscribeOrderBook(btcUsdt));

    // Wait for subscription callbacks
    for (int i = 0; i < 100; ++i) {
        if (binanceSubscribed && krakenSubscribed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(binanceSubscribed);
    EXPECT_TRUE(krakenSubscribed);
}

// Test order book updates
TEST_F(ApiTest, OrderBookUpdates) {
    bool binanceUpdates = false;
    bool krakenUpdates = false;
    TradingPair btcUsdt = TradingPair::BTC_USDT;

    // Get initial snapshots
    EXPECT_TRUE(binanceApi->getOrderBookSnapshot(btcUsdt));
    EXPECT_TRUE(krakenApi->getOrderBookSnapshot(btcUsdt));

    // Store initial order book states
    const auto& initialBinanceBook = orderBookManager->getOrderBook(btcUsdt);
    const auto& initialKrakenBook = orderBookManager->getOrderBook(btcUsdt);
    auto initialBinanceBids = initialBinanceBook.getBids();
    auto initialBinanceAsks = initialBinanceBook.getAsks();
    auto initialKrakenBids = initialKrakenBook.getBids();
    auto initialKrakenAsks = initialKrakenBook.getAsks();

    binanceApi->setUpdateCallback([&binanceUpdates]() {
        binanceUpdates = true;
    });

    krakenApi->setUpdateCallback([&krakenUpdates]() {
        krakenUpdates = true;
    });

    EXPECT_TRUE(binanceApi->subscribeOrderBook(btcUsdt));
    EXPECT_TRUE(krakenApi->subscribeOrderBook(btcUsdt));

    // Wait for updates
    for (int i = 0; i < 100; ++i) {
        if (binanceUpdates && krakenUpdates) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(binanceUpdates);
    EXPECT_TRUE(krakenUpdates);

    // Get updated order books
    const auto& updatedBinanceBook = orderBookManager->getOrderBook(btcUsdt);
    const auto& updatedKrakenBook = orderBookManager->getOrderBook(btcUsdt);

    // Verify that order books have been updated
    EXPECT_FALSE(updatedBinanceBook.getBids().empty());
    EXPECT_FALSE(updatedBinanceBook.getAsks().empty());
    EXPECT_FALSE(updatedKrakenBook.getBids().empty());
    EXPECT_FALSE(updatedKrakenBook.getAsks().empty());

    // Verify that at least some prices or quantities have changed
    bool binanceChanged = false;
    bool krakenChanged = false;

    // Check if Binance order book changed
    const auto& newBinanceBids = updatedBinanceBook.getBids();
    const auto& newBinanceAsks = updatedBinanceBook.getAsks();
    if (newBinanceBids.size() != initialBinanceBids.size() || 
        newBinanceAsks.size() != initialBinanceAsks.size()) {
        binanceChanged = true;
    } else {
        for (size_t i = 0; i < newBinanceBids.size() && !binanceChanged; ++i) {
            if (newBinanceBids[i].price != initialBinanceBids[i].price ||
                newBinanceBids[i].quantity != initialBinanceBids[i].quantity) {
                binanceChanged = true;
            }
        }
        for (size_t i = 0; i < newBinanceAsks.size() && !binanceChanged; ++i) {
            if (newBinanceAsks[i].price != initialBinanceAsks[i].price ||
                newBinanceAsks[i].quantity != initialBinanceAsks[i].quantity) {
                binanceChanged = true;
            }
        }
    }

    // Check if Kraken order book changed
    const auto& newKrakenBids = updatedKrakenBook.getBids();
    const auto& newKrakenAsks = updatedKrakenBook.getAsks();
    if (newKrakenBids.size() != initialKrakenBids.size() || 
        newKrakenAsks.size() != initialKrakenAsks.size()) {
        krakenChanged = true;
    } else {
        for (size_t i = 0; i < newKrakenBids.size() && !krakenChanged; ++i) {
            if (newKrakenBids[i].price != initialKrakenBids[i].price ||
                newKrakenBids[i].quantity != initialKrakenBids[i].quantity) {
                krakenChanged = true;
            }
        }
        for (size_t i = 0; i < newKrakenAsks.size() && !krakenChanged; ++i) {
            if (newKrakenAsks[i].price != initialKrakenAsks[i].price ||
                newKrakenAsks[i].quantity != initialKrakenAsks[i].quantity) {
                krakenChanged = true;
            }
        }
    }

    EXPECT_TRUE(binanceChanged) << "Binance order book did not change after updates";
    EXPECT_TRUE(krakenChanged) << "Kraken order book did not change after updates";
}

// Test order book snapshot
TEST_F(ApiTest, OrderBookSnapshot) {
    bool binanceSnapshotReceived = false;
    bool krakenSnapshotReceived = false;

    binanceApi->setSnapshotCallback([&binanceSnapshotReceived](bool success) {
        binanceSnapshotReceived = success;
    });

    krakenApi->setSnapshotCallback([&krakenSnapshotReceived](bool success) {
        krakenSnapshotReceived = success;
    });

    TradingPair btcUsdt = TradingPair::BTC_USDT;
    EXPECT_TRUE(binanceApi->getOrderBookSnapshot(btcUsdt));
    EXPECT_TRUE(krakenApi->getOrderBookSnapshot(btcUsdt));

    // Wait for snapshots
    for (int i = 0; i < 100; ++i) {
        if (binanceSnapshotReceived && krakenSnapshotReceived) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(binanceSnapshotReceived);
    EXPECT_TRUE(krakenSnapshotReceived);

    // Check order books
    const auto& binanceOrderBook = orderBookManager->getOrderBook(btcUsdt);
    const auto& krakenOrderBook = orderBookManager->getOrderBook(btcUsdt);

    EXPECT_FALSE(binanceOrderBook.getBids().empty());
    EXPECT_FALSE(binanceOrderBook.getAsks().empty());
    EXPECT_FALSE(krakenOrderBook.getBids().empty());
    EXPECT_FALSE(krakenOrderBook.getAsks().empty());
}

// Test connection management
TEST_F(ApiTest, ConnectionManagement) {
    EXPECT_FALSE(binanceApi->isConnected());
    EXPECT_FALSE(krakenApi->isConnected());

    EXPECT_TRUE(binanceApi->connect());
    EXPECT_TRUE(krakenApi->connect());

    // Wait for connection
    for (int i = 0; i < 100; ++i) {
        if (binanceApi->isConnected() && krakenApi->isConnected()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_TRUE(binanceApi->isConnected());
    EXPECT_TRUE(krakenApi->isConnected());

    binanceApi->disconnect();
    krakenApi->disconnect();

    // Wait for disconnection
    for (int i = 0; i < 100; ++i) {
        if (!binanceApi->isConnected() && !krakenApi->isConnected()) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    EXPECT_FALSE(binanceApi->isConnected());
    EXPECT_FALSE(krakenApi->isConnected());
}

// Test error handling
TEST_F(ApiTest, ErrorHandling) {
    TradingPair invalidPair = TradingPair::UNKNOWN;  // Invalid pair
    EXPECT_FALSE(binanceApi->subscribeOrderBook(invalidPair));
    EXPECT_FALSE(krakenApi->subscribeOrderBook(invalidPair));
    EXPECT_FALSE(binanceApi->getOrderBookSnapshot(invalidPair));
    EXPECT_FALSE(krakenApi->getOrderBookSnapshot(invalidPair));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 