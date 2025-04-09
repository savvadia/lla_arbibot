#include <gtest/gtest.h>
#include "mock_api.h"
#include "test_utils.h"
#include <memory>
#include <chrono>
#include <thread>
#include <iostream>
#include "test_utils.h"

class ApiTest : public ::testing::Test {
protected:
    void SetUp() override {
        std::cout << "\n[" << getTestTimestamp() << "] TEST: Setting up test..." << std::endl;
        orderBookManager = std::make_shared<OrderBookManager>();
        timersMgr = std::make_shared<TimersMgr>();
        std::cout << "[" << getTestTimestamp() << "] TEST: Creating MockApi..." << std::endl;
        mockApi = std::make_unique<MockApi>(*orderBookManager, *timersMgr, "Binance");
        std::cout << "[" << getTestTimestamp() << "] TEST: Test setup completed" << std::endl;
    }

    void TearDown() override {
        std::cout << "[" << getTestTimestamp() << "] TEST: Tearing down test..." << std::endl;
        mockApi->disconnect();
        std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for cleanup..." << std::endl;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        std::cout << "[" << getTestTimestamp() << "] TEST: Test teardown completed" << std::endl;
    }

    std::shared_ptr<OrderBookManager> orderBookManager;
    std::shared_ptr<TimersMgr> timersMgr;
    std::unique_ptr<MockApi> mockApi;
};

TEST_F(ApiTest, ConnectTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting ConnectTest..." << std::endl;
    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Checking connection status..." << std::endl;
    EXPECT_TRUE(mockApi->isConnected());
    std::cout << "[" << getTestTimestamp() << "] TEST: ConnectTest completed" << std::endl;
}

TEST_F(ApiTest, DisconnectTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting DisconnectTest..." << std::endl;
    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Calling disconnect()..." << std::endl;
    mockApi->disconnect();
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for disconnection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Checking connection status..." << std::endl;
    EXPECT_FALSE(mockApi->isConnected());
    std::cout << "[" << getTestTimestamp() << "] TEST: DisconnectTest completed" << std::endl;
}

TEST_F(ApiTest, SubscribeOrderBookTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting SubscribeOrderBookTest..." << std::endl;
    bool callbackCalled = false;
    std::cout << "[" << getTestTimestamp() << "] TEST: Setting up subscription callback..." << std::endl;
    mockApi->setSubscriptionCallback([&callbackCalled](bool success) {
        std::cout << "[" << getTestTimestamp() << "] TEST: Subscription callback called with success=" << success << std::endl;
        callbackCalled = success;
    });

    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Subscribing to order book..." << std::endl;
    EXPECT_TRUE(mockApi->subscribeOrderBook(TradingPair::BTC_USDT));
    
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for callback..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Checking callback status..." << std::endl;
    EXPECT_TRUE(callbackCalled);
    std::cout << "[" << getTestTimestamp() << "] TEST: SubscribeOrderBookTest completed" << std::endl;
}

TEST_F(ApiTest, GetOrderBookSnapshotTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting GetOrderBookSnapshotTest..." << std::endl;
    bool callbackCalled = false;
    mockApi->setSnapshotCallback([&callbackCalled](bool success) {
        std::cout << "[" << getTestTimestamp() << "] TEST: Snapshot callback called with success=" << success << std::endl;
        callbackCalled = success;
    });

    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Getting order book snapshot..." << std::endl;
    EXPECT_TRUE(mockApi->getOrderBookSnapshot(TradingPair::BTC_USDT));
    
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for callback..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Checking callback status..." << std::endl;
    EXPECT_TRUE(callbackCalled);
    std::cout << "[" << getTestTimestamp() << "] TEST: GetOrderBookSnapshotTest completed" << std::endl;
}

TEST_F(ApiTest, OrderBookUpdateTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting OrderBookUpdateTest..." << std::endl;
    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Subscribe to order book updates
    std::cout << "[" << getTestTimestamp() << "] TEST: Subscribing to order book..." << std::endl;
    EXPECT_TRUE(mockApi->subscribeOrderBook(TradingPair::BTC_USDT));
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for subscription..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Simulate an order book update
    std::vector<PriceLevel> bids = {
        {500.0, 1.0},
        {499.0, 2.0},
        {498.0, 3.0}
    };
    
    std::vector<PriceLevel> asks = {
        {501.0, 1.0},
        {502.0, 2.0},
        {503.0, 3.0}
    };
    
    std::cout << "[" << getTestTimestamp() << "] TEST: Simulating order book update..." << std::endl;
    mockApi->simulateOrderBookUpdate(bids, asks);
    
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for update processing..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Verify the order book was updated
    auto& orderBook = orderBookManager->getOrderBook(ExchangeId::BINANCE, TradingPair::BTC_USDT);
    EXPECT_EQ(orderBook.getBids().size(), 3);
    EXPECT_EQ(orderBook.getAsks().size(), 3);
    
    // Verify the order book contents
    const auto& orderBookBids = orderBook.getBids();
    const auto& orderBookAsks = orderBook.getAsks();
    
    for (size_t i = 0; i < 3; ++i) {
        EXPECT_EQ(orderBookBids[i].price, bids[i].price);
        EXPECT_EQ(orderBookBids[i].quantity, bids[i].quantity);
        EXPECT_EQ(orderBookAsks[i].price, asks[i].price);
        EXPECT_EQ(orderBookAsks[i].quantity, asks[i].quantity);
    }
    
    std::cout << "[" << getTestTimestamp() << "] TEST: OrderBookUpdateTest completed" << std::endl;
}

TEST_F(ApiTest, PlaceOrderTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting PlaceOrderTest..." << std::endl;
    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Placing order..." << std::endl;
    EXPECT_TRUE(mockApi->placeOrder(TradingPair::BTC_USDT, OrderType::BUY, 50000.0, 1.0));
    std::cout << "[" << getTestTimestamp() << "] TEST: PlaceOrderTest completed" << std::endl;
}

TEST_F(ApiTest, CancelOrderTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting CancelOrderTest..." << std::endl;
    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Canceling order..." << std::endl;
    EXPECT_TRUE(mockApi->cancelOrder("test-order-id"));
    std::cout << "[" << getTestTimestamp() << "] TEST: CancelOrderTest completed" << std::endl;
}

TEST_F(ApiTest, GetBalanceTest) {
    std::cout << "\n[" << getTestTimestamp() << "] TEST: Starting GetBalanceTest..." << std::endl;
    std::cout << "[" << getTestTimestamp() << "] TEST: Calling connect()..." << std::endl;
    EXPECT_TRUE(mockApi->connect());
    std::cout << "[" << getTestTimestamp() << "] TEST: Waiting for connection..." << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::cout << "[" << getTestTimestamp() << "] TEST: Getting balance..." << std::endl;
    EXPECT_TRUE(mockApi->getBalance("BTC"));
    std::cout << "[" << getTestTimestamp() << "] TEST: GetBalanceTest completed" << std::endl;
}

int main(int argc, char **argv) {
    std::cout << "[" << getTestTimestamp() << "] TEST: Starting test suite..." << std::endl;
    ::testing::InitGoogleTest(&argc, argv);
    int result = RUN_ALL_TESTS();
    std::cout << "[" << getTestTimestamp() << "] TEST: Test suite completed with result: " << result << std::endl;
    return result;
} 