#include <gtest/gtest.h>
#include "../src/orderbook_mgr.h"
#include "../src/tracer.h"
#include <vector>
#include <thread>
#include <chrono>

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

        // Enable logging for order book tests
        FastTraceLogger::setLoggingEnabled(true);
        FastTraceLogger::setLoggingEnabled(TraceInstance::ORDERBOOK, true);
        FastTraceLogger::setLoggingEnabled(TraceInstance::EX_MGR, true);
        FastTraceLogger::setLoggingEnabled(TraceInstance::BALANCE, true);
        FastTraceLogger::setLoggingEnabled(TraceInstance::EVENT_LOOP, true);
        FastTraceLogger::setLoggingEnabled(TraceInstance::TIMER, true);
    }

    void TearDown() override {
        // Disable logging after tests
        FastTraceLogger::setLoggingEnabled(false);
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
    auto result = book.update(TradingPair::BTC_USDT, testBids, testAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    
    EXPECT_EQ(book.getBestBid(), 50000.0);
    EXPECT_EQ(book.getBestAsk(), 50100.0);
    EXPECT_EQ(book.getBestBidQuantity(), 1.0);
    EXPECT_EQ(book.getBestAskQuantity(), 1.0);
}

// Test order book sorting
TEST_F(OrderBookTest, OrderBookSorting) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add bids in random order
    std::vector<PriceLevel> bids = {
        {50000.0, 1.0},
        {49800.0, 1.0},
        {49900.0, 1.0}
    };
    
    // Add asks in random order
    std::vector<PriceLevel> asks = {
        {50200.0, 1.0},
        {50100.0, 1.0},
        {50300.0, 1.0}
    };
    
    auto result = book.update(TradingPair::BTC_USDT, bids, asks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    
    // Verify sorting
    EXPECT_EQ(book.getBestBid(), 50000.0);
    EXPECT_EQ(book.getBestAsk(), 50100.0);
}

// Test order book depth
TEST_F(OrderBookTest, OrderBookDepth) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add multiple price levels with small quantities to fit within MAX_ORDER_BOOK_AMOUNT
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    for (int i = 0; i < 10; ++i) {
        bids.push_back({50000.0 - i * 100.0, 0.1});  // Small quantity to fit more levels
        asks.push_back({50100.0 + i * 100.0, 0.1});  // Small quantity to fit more levels
    }
    auto result = book.update(TradingPair::BTC_USDT, bids, asks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    
    auto [currentBids, currentAsks] = book.getState();
    EXPECT_EQ(currentBids.size(), 10);
    EXPECT_EQ(currentAsks.size(), 10);
}

// Test lastUpdate timestamp behavior
TEST_F(OrderBookTest, LastUpdateTimestamp) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Initial update
    std::vector<PriceLevel> initialBids = {{50000.0, 1.0}};
    std::vector<PriceLevel> initialAsks = {{50100.0, 1.0}};
    auto result = book.update(TradingPair::BTC_USDT, initialBids, initialAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    
    auto initialData = book.getOrderBookData();
    
    // Update with same prices and quantities - should change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Longer delay
    result = book.update(TradingPair::BTC_USDT, initialBids, initialAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::NO_CHANGES_TO_BEST_PRICES);
    
    auto sameData = book.getOrderBookData();
    EXPECT_GT(sameData.lastUpdate, initialData.lastUpdate);
    
    // Update with different quantity - should change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Longer delay
    std::vector<PriceLevel> newBids = {{50000.0, 2.0}};  // Different quantity
    result = book.update(TradingPair::BTC_USDT, newBids, initialAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::NO_CHANGES_TO_BEST_PRICES);
    auto quantityData = book.getOrderBookData();
    EXPECT_GT(quantityData.lastUpdate, initialData.lastUpdate);
    
    // Update with different price - should change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Longer delay
    std::vector<PriceLevel> newPriceBids = {{50001.0, 2.0}};  // Different price
    result = book.update(TradingPair::BTC_USDT, newPriceBids, initialAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    auto priceData = book.getOrderBookData();
    EXPECT_GT(priceData.lastUpdate, quantityData.lastUpdate);
    
    // Update with same values again - should change lastUpdate
    std::this_thread::sleep_for(std::chrono::milliseconds(100));  // Longer delay
    result = book.update(TradingPair::BTC_USDT, newPriceBids, initialAsks);  // Same values
    auto finalData = book.getOrderBookData();
    EXPECT_GT(finalData.lastUpdate, priceData.lastUpdate);
}

// Test edge cases for price levels
TEST_F(OrderBookTest, PriceLevelEdgeCases) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Test very small quantities
    std::vector<PriceLevel> smallBids = {{500.0, 1e-8}};
    std::vector<PriceLevel> emptyAsks;
    auto result = book.update(TradingPair::BTC_USDT, smallBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    EXPECT_NEAR(book.getBestBidQuantity(), 1e-8, 1e-10);  // Use NEAR for floating point comparison
    EXPECT_NEAR(book.getBestBid(), 500.0, 1e-10);
    
    // Test very large prices
    std::vector<PriceLevel> largeBids = {{1000.0, 1.0}};
    result = book.update(TradingPair::BTC_USDT, largeBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    EXPECT_EQ(book.getBestBid(), 1000.0);  // Should be the best bid since it's higher
    
    // Test zero price (should be rejected)
    std::vector<PriceLevel> zeroBids = {{0.0, 1.0}};
    result = book.update(TradingPair::BTC_USDT, zeroBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::NO_CHANGES_TO_BEST_PRICES);
    EXPECT_EQ(book.getBestBid(), 1000.0);  // Should remain unchanged
    
    // Test negative quantity (should be rejected)
    std::vector<PriceLevel> negBids = {{500.0, -1.0}};
    result = book.update(TradingPair::BTC_USDT, negBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::NO_CHANGES_TO_BEST_PRICES);
    EXPECT_EQ(book.getBestBid(), 1000.0);  // Should remain unchanged
    
    // Remove the highest bid
    std::vector<PriceLevel> removeBids = {{1000.0, 0.0}};
    result = book.update(TradingPair::BTC_USDT, removeBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    EXPECT_NEAR(book.getBestBid(), 500.0, 1e-10);  // Should fall back to the previous bid
    
    // Test very small price changes
    std::vector<PriceLevel> smallPriceBids = {{500.00000001, 1.0}};
    result = book.update(TradingPair::BTC_USDT, smallPriceBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    EXPECT_NEAR(book.getBestBid(), 500.00000001, 1e-10);
    
    // Test removing level with very small quantity
    std::vector<PriceLevel> smallQtyBids = {{500.00000001, 1e-8}};
    result = book.update(TradingPair::BTC_USDT, smallQtyBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::NO_CHANGES_TO_BEST_PRICES);
    EXPECT_NEAR(book.getBestBidQuantity(), 1e-8, 1e-10);
    
    // Test very small quantities for asks
    std::vector<PriceLevel> smallAsks = {{501.0, 1e-8}};
    result = book.update(TradingPair::BTC_USDT, smallQtyBids, smallAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    EXPECT_NEAR(book.getBestAskQuantity(), 1e-8, 1e-10);
    EXPECT_NEAR(book.getBestAsk(), 501.0, 1e-10);
    
    // Test very small price changes for asks
    std::vector<PriceLevel> smallPriceAsks = {{501.00000001, 1.0}};
    result = book.update(TradingPair::BTC_USDT, smallQtyBids, smallPriceAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    EXPECT_NEAR(book.getBestAsk(), 501.0, 1e-10);  // Lower price should be the best ask
}

// Test order book state transitions
TEST_F(OrderBookTest, StateTransitions) {
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    
    // Add initial price levels
    std::vector<PriceLevel> initialBids = {{500.0, 1.0}};
    std::vector<PriceLevel> initialAsks = {{501.0, 1.0}};
    auto result = book.update(TradingPair::BTC_USDT, initialBids, initialAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    
    // Remove all price levels
    std::vector<PriceLevel> emptyBids = {{500.0, 0.0}};
    std::vector<PriceLevel> emptyAsks = {{501.0, 0.0}};
    result = book.update(TradingPair::BTC_USDT, emptyBids, emptyAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    
    // Verify empty state
    EXPECT_EQ(book.getBestBid(), 0.0);
    EXPECT_EQ(book.getBestAsk(), 0.0);
    EXPECT_EQ(book.getBestBidQuantity(), 0.0);
    EXPECT_EQ(book.getBestAskQuantity(), 0.0);
    
    // Add new price levels
    std::vector<PriceLevel> newBids = {{510.0, 1.0}};
    std::vector<PriceLevel> newAsks = {{511.0, 1.0}};
    result = book.update(TradingPair::BTC_USDT, newBids, newAsks);
    EXPECT_EQ(result, OrderBook::UpdateOutcome::BEST_PRICES_CHANGED);
    
    // Verify new state
    EXPECT_EQ(book.getBestBid(), 510.0);
    EXPECT_EQ(book.getBestAsk(), 511.0);
}

// Test concurrent updates to OrderBookManager
TEST_F(OrderBookTest, OrderBookManagerMultiPair) {
    OrderBookManager manager;
    
    // Update order books for different pairs
    std::vector<PriceLevel> btcBids = {{50000.0, 1.0}};
    std::vector<PriceLevel> emptyAsks;
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT, btcBids, emptyAsks);
    
    std::vector<PriceLevel> ethBids = {{3000.0, 1.0}};
    manager.updateOrderBook(ExchangeId::BINANCE, TradingPair::ETH_USDT, ethBids, emptyAsks);
    
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

// Test merge functionality
TEST_F(OrderBookTest, MergeEmptyLists) {
    std::vector<PriceLevel> oldList;
    std::vector<PriceLevel> newList;
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    book.mergeSortedLists(oldList, newList, true);
    EXPECT_TRUE(oldList.empty());
    EXPECT_TRUE(OrderBook::isSorted(oldList, true));
}

TEST_F(OrderBookTest, MergeEmptyOldList) {
    std::vector<PriceLevel> oldList;
    std::vector<PriceLevel> newList = {{50000.0, 1.0}};
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    book.mergeSortedLists(oldList, newList, true);
    EXPECT_EQ(oldList.size(), 1);
    EXPECT_EQ(oldList[0].price, 50000.0);
}

TEST_F(OrderBookTest, MergeEmptyNewList) {
    std::vector<PriceLevel> oldList = {{50000.0, 1.0}};
    std::vector<PriceLevel> newList;
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    book.mergeSortedLists(oldList, newList, true);
    EXPECT_EQ(oldList.size(), 1);
    EXPECT_EQ(oldList[0].price, 50000.0);
}

TEST_F(OrderBookTest, MergeWithUpdates) {
    std::vector<PriceLevel> oldList = {{50000.0, 1.0}, {49900.0, 2.0}};
    std::vector<PriceLevel> newList = {{50000.0, 2.0}, {49800.0, 3.0}};
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    book.mergeSortedLists(oldList, newList, true);
    EXPECT_EQ(oldList.size(), 3);
    EXPECT_EQ(oldList[0].price, 50000.0);
    EXPECT_EQ(oldList[0].quantity, 2.0);
}

TEST_F(OrderBookTest, MergeWithZeroQuantitiesInNewList) {
    std::vector<PriceLevel> oldList = {{50000.0, 1.0}};
    std::vector<PriceLevel> newList = {{50000.0, 0.0}};
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    book.mergeSortedLists(oldList, newList, true);
    EXPECT_EQ(oldList.size(), 0);
}

TEST_F(OrderBookTest, MergeWithZeroQuantitiesInOldList) {
    std::vector<PriceLevel> oldList = {{50000.0, 0.0}};
    std::vector<PriceLevel> newList = {{50000.0, 1.0}};
    OrderBook book(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    book.mergeSortedLists(oldList, newList, true);
    EXPECT_EQ(oldList.size(), 1);
    EXPECT_EQ(oldList[0].price, 50000.0);
    EXPECT_EQ(oldList[0].quantity, 1.0);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 