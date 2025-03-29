#include <gtest/gtest.h>
#include "../src/orderbook.h"
#include "../src/tracer.h"
#include <vector>
#include <thread>
#include <chrono>
#include <atomic>

using namespace std;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize test data
        testBids = {
            {50000.0, 1.0},
            {49900.0, 2.0},
            {49800.0, 3.0}
        };
        testAsks = {
            {50100.0, 1.0},
            {50200.0, 2.0},
            {50300.0, 3.0}
        };
    }

    std::vector<PriceLevel> testBids;
    std::vector<PriceLevel> testAsks;
};

// Test basic order book initialization and updates
TEST_F(OrderBookTest, BasicInitializationAndUpdates) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Test initial state
    EXPECT_EQ(book.getBestBid(), 0.0);
    EXPECT_EQ(book.getBestAsk(), 0.0);
    EXPECT_EQ(book.getBestBidQuantity(), 0.0);
    EXPECT_EQ(book.getBestAskQuantity(), 0.0);
    
    // Test update with initial data
    book.update(testBids, testAsks);
    
    EXPECT_EQ(book.getBestBid(), 50000.0);
    EXPECT_EQ(book.getBestAsk(), 50100.0);
    EXPECT_EQ(book.getBestBidQuantity(), 1.0);
    EXPECT_EQ(book.getBestAskQuantity(), 1.0);
}

// Test price level updates
TEST_F(OrderBookTest, PriceLevelUpdates) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Test single price level updates
    book.updatePriceLevel(true, 50000.0, 1.0);  // Add bid
    EXPECT_EQ(book.getBestBid(), 50000.0);
    EXPECT_EQ(book.getBestBidQuantity(), 1.0);
    
    book.updatePriceLevel(false, 50100.0, 1.0);  // Add ask
    EXPECT_EQ(book.getBestAsk(), 50100.0);
    EXPECT_EQ(book.getBestAskQuantity(), 1.0);
    
    // Test updating existing levels
    book.updatePriceLevel(true, 50000.0, 2.0);  // Update bid
    EXPECT_EQ(book.getBestBidQuantity(), 2.0);
    
    book.updatePriceLevel(false, 50100.0, 2.0);  // Update ask
    EXPECT_EQ(book.getBestAskQuantity(), 2.0);
    
    // Test removing levels
    book.updatePriceLevel(true, 50000.0, 0.0);  // Remove bid
    EXPECT_EQ(book.getBestBid(), 0.0);
    EXPECT_EQ(book.getBestBidQuantity(), 0.0);
    
    book.updatePriceLevel(false, 50100.0, 0.0);  // Remove ask
    EXPECT_EQ(book.getBestAsk(), 0.0);
    EXPECT_EQ(book.getBestAskQuantity(), 0.0);
}

// Test order book sorting
TEST_F(OrderBookTest, OrderBookSorting) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add bids in random order
    book.updatePriceLevel(true, 50000.0, 1.0);
    book.updatePriceLevel(true, 49800.0, 1.0);
    book.updatePriceLevel(true, 49900.0, 1.0);
    
    // Add asks in random order
    book.updatePriceLevel(false, 50200.0, 1.0);
    book.updatePriceLevel(false, 50100.0, 1.0);
    book.updatePriceLevel(false, 50300.0, 1.0);
    
    // Verify sorting
    EXPECT_EQ(book.getBestBid(), 50000.0);
    EXPECT_EQ(book.getBestAsk(), 50100.0);
}

// Test concurrent updates
TEST_F(OrderBookTest, ConcurrentUpdates) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Create multiple threads for concurrent updates
    std::vector<std::thread> threads;
    for (int i = 0; i < 10; ++i) {
        threads.emplace_back([&book, i]() {
            for (int j = 0; j < 100; ++j) {
                book.updatePriceLevel(true, 50000.0 + i, 1.0);
                book.updatePriceLevel(false, 50100.0 + i, 1.0);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify final state
    EXPECT_GT(book.getBestBid(), 0.0);
    EXPECT_GT(book.getBestAsk(), 0.0);
}

// Test OrderBookManager
TEST_F(OrderBookTest, OrderBookManager) {
    OrderBookManager manager;
    
    // Test callback registration
    bool callbackCalled = false;
    manager.setUpdateCallback([&callbackCalled](ExchangeId, TradingPair, const OrderBook&) {
        callbackCalled = true;
    });
    
    // Test order book updates
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, testBids, testAsks);
    EXPECT_TRUE(callbackCalled);
    
    // Test single price level updates
    callbackCalled = false;
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, 50000.0, 1.0, true);
    EXPECT_TRUE(callbackCalled);
    
    // Test order book retrieval
    auto& book = manager.getOrderBook(TradingPair::BTC_USDT);
    EXPECT_EQ(book.getExchangeId(), ExchangeId::BINANCE);
    EXPECT_EQ(book.getTradingPair(), TradingPair::BTC_USDT);
}

// Test order book state consistency
TEST_F(OrderBookTest, StateConsistency) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add multiple price levels
    std::vector<PriceLevel> bids = {
        {50000.0, 1.0},
        {49900.0, 2.0},
        {49800.0, 3.0}
    };
    std::vector<PriceLevel> asks = {
        {50100.0, 1.0},
        {50200.0, 2.0},
        {50300.0, 3.0}
    };
    
    book.update(bids, asks);
    
    // Verify state consistency
    auto [currentBids, currentAsks] = book.getState();
    EXPECT_EQ(currentBids.size(), 3);
    EXPECT_EQ(currentAsks.size(), 3);
    
    // Verify bid order (descending)
    for (size_t i = 1; i < currentBids.size(); ++i) {
        EXPECT_GT(currentBids[i-1].price, currentBids[i].price);
    }
    
    // Verify ask order (ascending)
    for (size_t i = 1; i < currentAsks.size(); ++i) {
        EXPECT_LT(currentAsks[i-1].price, currentAsks[i].price);
    }
}

// Test order book spread calculation
TEST_F(OrderBookTest, SpreadCalculation) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    book.updatePriceLevel(true, 50000.0, 1.0);  // Best bid
    book.updatePriceLevel(false, 50100.0, 1.0); // Best ask
    
    double spread = book.getBestAsk() - book.getBestBid();
    EXPECT_EQ(spread, 100.0);
}

// Test order book depth
TEST_F(OrderBookTest, OrderBookDepth) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add multiple price levels
    for (int i = 0; i < 10; ++i) {
        book.updatePriceLevel(true, 50000.0 - i * 100.0, 1.0);
        book.updatePriceLevel(false, 50100.0 + i * 100.0, 1.0);
    }
    
    auto [bids, asks] = book.getState();
    EXPECT_EQ(bids.size(), 10);
    EXPECT_EQ(asks.size(), 10);
}

// Test lastUpdate timestamp behavior
TEST_F(OrderBookTest, LastUpdateTimestamp) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Initial update
    book.updatePriceLevel(true, 50000.0, 1.0);
    book.updatePriceLevel(false, 50100.0, 1.0);
    
    auto initialData = book.getOrderBookData();
    
    // Update with same prices and quantities - should not change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    book.updatePriceLevel(true, 50000.0, 1.0);
    book.updatePriceLevel(false, 50100.0, 1.0);
    
    auto sameData = book.getOrderBookData();
    EXPECT_EQ(sameData.lastUpdate, initialData.lastUpdate);
    
    // Update with different quantity - should change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    book.updatePriceLevel(true, 50000.0, 2.0);
    auto quantityData = book.getOrderBookData();
    EXPECT_GT(quantityData.lastUpdate, initialData.lastUpdate);
    
    // Update with different price - should change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    book.updatePriceLevel(true, 50001.0, 2.0);
    auto priceData = book.getOrderBookData();
    EXPECT_GT(priceData.lastUpdate, quantityData.lastUpdate);
    
    // Update with same values again - should not change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    book.updatePriceLevel(true, 50001.0, 2.0);
    auto finalData = book.getOrderBookData();
    EXPECT_EQ(finalData.lastUpdate, priceData.lastUpdate);
}

// Test edge cases for price levels
TEST_F(OrderBookTest, PriceLevelEdgeCases) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Test very small quantities
    book.updatePriceLevel(true, 50000.0, 1e-8);
    EXPECT_NEAR(book.getBestBidQuantity(), 1e-8, 1e-10);  // Use NEAR for floating point comparison
    EXPECT_NEAR(book.getBestBid(), 50000.0, 1e-10);
    
    // Test very large prices
    book.updatePriceLevel(true, 1000000.0, 1.0);
    EXPECT_EQ(book.getBestBid(), 1000000.0);  // Should be the best bid since it's higher
    
    // Test zero price (should be rejected)
    book.updatePriceLevel(true, 0.0, 1.0);
    EXPECT_EQ(book.getBestBid(), 1000000.0);  // Should remain unchanged
    
    // Test negative quantity (should be rejected)
    book.updatePriceLevel(true, 50000.0, -1.0);
    EXPECT_EQ(book.getBestBid(), 1000000.0);  // Should remain unchanged
    
    // Remove the highest bid
    book.updatePriceLevel(true, 1000000.0, 0.0);
    EXPECT_NEAR(book.getBestBid(), 50000.0, 1e-10);  // Should fall back to the previous bid
    
    // Test very small price changes
    book.updatePriceLevel(true, 50000.00000001, 1.0);
    EXPECT_NEAR(book.getBestBid(), 50000.00000001, 1e-10);
    
    // Test removing level with very small quantity
    book.updatePriceLevel(true, 50000.00000001, 1e-8);
    EXPECT_NEAR(book.getBestBidQuantity(), 1e-8, 1e-10);
    
    // Remove all bids
    book.updatePriceLevel(true, 50000.00000001, 0.0);
    book.updatePriceLevel(true, 50000.0, 0.0);
    EXPECT_EQ(book.getBestBid(), 0.0);
    
    // Test very small quantities for asks
    book.updatePriceLevel(false, 50100.0, 1e-8);
    EXPECT_NEAR(book.getBestAskQuantity(), 1e-8, 1e-10);
    EXPECT_NEAR(book.getBestAsk(), 50100.0, 1e-10);
    
    // Test very small price changes for asks
    book.updatePriceLevel(false, 50100.00000001, 1.0);
    EXPECT_NEAR(book.getBestAsk(), 50100.0, 1e-10);  // Lower price should be the best ask
}

// Test order book state transitions
TEST_F(OrderBookTest, StateTransitions) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add initial price levels
    book.updatePriceLevel(true, 50000.0, 1.0);
    book.updatePriceLevel(false, 50100.0, 1.0);
    
    // Remove all price levels
    book.updatePriceLevel(true, 50000.0, 0.0);
    book.updatePriceLevel(false, 50100.0, 0.0);
    
    // Verify empty state
    EXPECT_EQ(book.getBestBid(), 0.0);
    EXPECT_EQ(book.getBestAsk(), 0.0);
    EXPECT_EQ(book.getBestBidQuantity(), 0.0);
    EXPECT_EQ(book.getBestAskQuantity(), 0.0);
    
    // Add new price levels
    book.updatePriceLevel(true, 51000.0, 1.0);
    book.updatePriceLevel(false, 51100.0, 1.0);
    
    // Verify new state
    EXPECT_EQ(book.getBestBid(), 51000.0);
    EXPECT_EQ(book.getBestAsk(), 51100.0);
}

// Test concurrent updates to OrderBookManager
TEST_F(OrderBookTest, OrderBookManagerConcurrency) {
    OrderBookManager manager;
    std::atomic<int> callbackCount{0};
    
    // Set callback that counts invocations
    manager.setUpdateCallback([&callbackCount](ExchangeId, TradingPair, const OrderBook&) {
        callbackCount++;
    });
    
    // Create multiple threads for concurrent updates
    std::vector<std::thread> threads;
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back([&manager, i]() {
            for (int j = 0; j < 100; ++j) {
                manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, 
                    std::vector<PriceLevel>{{50000.0 + i, 1.0}}, 
                    std::vector<PriceLevel>{{50100.0 + i, 1.0}});
                
                // Add some random delay to increase chance of race conditions
                if (j % 10 == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify final state
    auto& book = manager.getOrderBook(TradingPair::BTC_USDT);
    EXPECT_GT(book.getBestBid(), 0.0);
    EXPECT_GT(book.getBestAsk(), 0.0);
    EXPECT_EQ(callbackCount.load(), 500); // 5 threads * 100 updates each
}

// Test OrderBookManager edge cases
TEST_F(OrderBookTest, OrderBookManagerEdgeCases) {
    OrderBookManager manager;
    std::atomic<int> callbackCount{0};
    
    // Set callback that counts invocations
    manager.setUpdateCallback([&callbackCount](ExchangeId, TradingPair, const OrderBook&) {
        callbackCount++;
    });
    
    // Test multiple trading pairs
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, testBids, testAsks);
    manager.updateOrderBook(ExchangeId::KRAKEN, TradingPair::ETH_USDT, testBids, testAsks);
    
    auto& btcBook = manager.getOrderBook(TradingPair::BTC_USDT);
    auto& ethBook = manager.getOrderBook(TradingPair::ETH_USDT);
    
    EXPECT_EQ(btcBook.getTradingPair(), TradingPair::BTC_USDT);
    EXPECT_EQ(ethBook.getTradingPair(), TradingPair::ETH_USDT);
    
    // Multiple updates to same pair
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, testBids, testAsks);
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, testBids, testAsks);
    
    // Multiple updates to different pairs
    manager.updateOrderBook(ExchangeId::KRAKEN, TradingPair::ETH_USDT, testBids, testAsks);
    manager.updateOrderBook(ExchangeId::KRAKEN, TradingPair::ETH_USDT, testBids, testAsks);
    
    EXPECT_EQ(callbackCount.load(), 6);  // 2 initial + 2 BTC + 2 ETH updates
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 