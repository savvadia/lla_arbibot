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
    bool callbackCalled = false;
    
    // Set callback
    manager.setUpdateCallback([&callbackCalled](ExchangeId, TradingPair) {
        callbackCalled = true;
    });
    
    // Update order book
    std::vector<PriceLevel> bids = {{50000.0, 1.0}};
    std::vector<PriceLevel> asks;
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, bids, asks);
    EXPECT_TRUE(callbackCalled);
    
    // Test order book retrieval
    auto& book = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
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
    
    std::vector<PriceLevel> bids = {{50000.0, 1.0}};  // Best bid
    std::vector<PriceLevel> asks = {{50100.0, 1.0}}; // Best ask
    book.update(bids, asks);
    
    double spread = book.getBestAsk() - book.getBestBid();
    EXPECT_EQ(spread, 100.0);
}

// Test order book depth
TEST_F(OrderBookTest, OrderBookDepth) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add multiple price levels
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    for (int i = 0; i < 10; ++i) {
        bids.push_back({50000.0 - i * 100.0, 1.0});
        asks.push_back({50100.0 + i * 100.0, 1.0});
    }
    book.update(bids, asks);
    
    auto [currentBids, currentAsks] = book.getState();
    EXPECT_EQ(currentBids.size(), 10);
    EXPECT_EQ(currentAsks.size(), 10);
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
    manager.setUpdateCallback([&callbackCount](ExchangeId, TradingPair) {
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
                
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify final state
    auto& book = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    EXPECT_GT(book.getBestBid(), 0.0);
    EXPECT_GT(book.getBestAsk(), 0.0);
    EXPECT_EQ(callbackCount.load(), 500); // 5 threads * 100 updates each
}

// Test edge cases for OrderBookManager
TEST_F(OrderBookTest, OrderBookManagerEdgeCases) {
    OrderBookManager manager;
    
    // Test very small quantity
    std::vector<PriceLevel> bids = {{50000.0, 1e-8}};
    std::vector<PriceLevel> asks;
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, bids, asks);
    auto& book1 = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    EXPECT_EQ(book1.getBestBidQuantity(), 1e-8);
    
    // Test very high price
    bids = {{1000000.0, 1.0}};
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, bids, asks);
    auto& book2 = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    EXPECT_EQ(book2.getBestBid(), 1000000.0);
    
    // Test zero price
    bids = {{0.0, 1.0}};
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, bids, asks);
    auto& book3 = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    EXPECT_EQ(book3.getBestBid(), 0.0);
}

// Test multi-exchange order book management
TEST_F(OrderBookTest, OrderBookManagerMultiExchange) {
    OrderBookManager manager;
    
    // Update order books for different exchanges
    std::vector<PriceLevel> bids = {{50000.0, 1.0}};
    std::vector<PriceLevel> asks;
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, bids, asks);
    
    bids = {{50100.0, 1.0}};
    manager.updateOrderBook(ExchangeId::KRAKEN, TradingPair::BTC_USDT, bids, asks);
    
    // Verify order books are independent
    auto& binanceBook = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    auto& krakenBook = manager.getOrderBook(ExchangeId::KRAKEN, TradingPair::BTC_USDT);
    
    EXPECT_EQ(binanceBook.getBestBid(), 50000.0);
    EXPECT_EQ(krakenBook.getBestBid(), 50100.0);
}

// Test multi-pair order book management
TEST_F(OrderBookTest, OrderBookManagerMultiPair) {
    OrderBookManager manager;
    
    // Update order books for different pairs
    std::vector<PriceLevel> bids = {{50000.0, 1.0}};
    std::vector<PriceLevel> asks;
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, bids, asks);
    
    bids = {{3000.0, 1.0}};
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::ETH_USDT, bids, asks);
    
    // Verify order books are independent
    auto& btcBook = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    auto& ethBook = manager.getOrderBook(ExchangeId::BINANCE, TradingPair::ETH_USDT);
    
    EXPECT_EQ(btcBook.getBestBid(), 50000.0);
    EXPECT_EQ(ethBook.getBestBid(), 3000.0);
}

// Helper function to create a test price level
PriceLevel makePriceLevel(double price, double quantity) {
    return PriceLevel{price, quantity};
}

// Helper function to check if a list is sorted correctly
bool isSorted(const std::vector<PriceLevel>& list, bool isBid) {
    if (list.empty()) return true;
    
    for (size_t i = 1; i < list.size(); ++i) {
        if (isBid) {
            if (list[i-1].price <= list[i].price) return false;
        } else {
            if (list[i-1].price >= list[i].price) return false;
        }
    }
    return true;
}

TEST(OrderBookTest, MergeEmptyLists) {
    std::vector<PriceLevel> oldList;
    std::vector<PriceLevel> newList;
    double totalAmount = 0.0;
    
    mergeSortedLists(oldList, newList, true, totalAmount);
    EXPECT_TRUE(oldList.empty());
    EXPECT_EQ(totalAmount, 0.0);
}

TEST(OrderBookTest, MergeEmptyOldList) {
    std::vector<PriceLevel> oldList;
    std::vector<PriceLevel> newList = {
        makePriceLevel(100.0, 1.0),
        makePriceLevel(99.0, 2.0),
        makePriceLevel(98.0, 3.0)
    };
    double totalAmount = 0.0;
    
    mergeSortedLists(oldList, newList, true, totalAmount);
    EXPECT_EQ(oldList.size(), 3);
    EXPECT_TRUE(isSorted(oldList, true));
    EXPECT_EQ(totalAmount, 100.0 + 99.0*2.0 + 98.0*3.0);
}

TEST(OrderBookTest, MergeEmptyNewList) {
    std::vector<PriceLevel> oldList = {
        makePriceLevel(100.0, 1.0),
        makePriceLevel(99.0, 2.0),
        makePriceLevel(98.0, 3.0)
    };
    std::vector<PriceLevel> newList;
    double totalAmount = 0.0;
    
    mergeSortedLists(oldList, newList, true, totalAmount);
    EXPECT_EQ(oldList.size(), 3);
    EXPECT_TRUE(isSorted(oldList, true));
    EXPECT_EQ(totalAmount, 100.0 + 99.0*2.0 + 98.0*3.0);
}

TEST(OrderBookTest, MergeWithUpdates) {
    std::vector<PriceLevel> oldList = {
        makePriceLevel(100.0, 1.0),
        makePriceLevel(99.0, 2.0),
        makePriceLevel(98.0, 3.0)
    };
    std::vector<PriceLevel> newList = {
        makePriceLevel(101.0, 1.5),  // New best bid
        makePriceLevel(99.0, 3.0),   // Update existing price
        makePriceLevel(97.0, 4.0)    // New worst bid
    };
    double totalAmount = 0.0;
    
    mergeSortedLists(oldList, newList, true, totalAmount);
    EXPECT_EQ(oldList.size(), 4);
    EXPECT_TRUE(isSorted(oldList, true));
    
    // Check specific updates
    EXPECT_EQ(oldList[3].price, 101.0);  // Best bid
    EXPECT_EQ(oldList[3].quantity, 1.5);
    EXPECT_EQ(oldList[1].price, 99.0);   // Updated price
    EXPECT_EQ(oldList[1].quantity, 3.0);
    EXPECT_EQ(oldList[0].price, 97.0);   // Worst bid
    EXPECT_EQ(oldList[0].quantity, 4.0);
}

TEST(OrderBookTest, MergeWithZeroQuantities) {
    std::vector<PriceLevel> oldList = {
        makePriceLevel(100.0, 1.0),
        makePriceLevel(99.0, 2.0),
        makePriceLevel(98.0, 3.0)
    };
    std::vector<PriceLevel> newList = {
        makePriceLevel(99.0, 0.0),   // Remove price level
        makePriceLevel(97.0, 4.0)    // New price level
    };
    double totalAmount = 0.0;
    
    mergeSortedLists(oldList, newList, true, totalAmount);
    EXPECT_EQ(oldList.size(), 3);  // One removed, one added
    EXPECT_TRUE(isSorted(oldList, true));
    
    // Check that price level 99.0 was removed
    for (const auto& level : oldList) {
        EXPECT_NE(level.price, 99.0);
    }
}

TEST(OrderBookTest, MergeWithSizeLimit) {
    std::vector<PriceLevel> oldList = {
        makePriceLevel(100.0, 1.0),
        makePriceLevel(99.0, 2.0),
        makePriceLevel(98.0, 3.0)
    };
    std::vector<PriceLevel> newList = {
        makePriceLevel(101.0, 1.5),
        makePriceLevel(99.0, 3.0),
        makePriceLevel(97.0, 4.0)
    };
    double totalAmount = 0.0;
    
    // Use a small limit for testing
    const double testLimit = 200.0;  // This will keep only the best prices
    
    mergeSortedLists(oldList, newList, true, totalAmount, testLimit);
    EXPECT_LE(totalAmount, testLimit);
    EXPECT_TRUE(isSorted(oldList, true));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 