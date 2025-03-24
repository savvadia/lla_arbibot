#include <gtest/gtest.h>
#include "../src/timers.h"
#include <thread>
#include <chrono>

struct TestOrder {
    bool fired = false;
};

void testCallback(int id, void* data) {
    TestOrder* order = static_cast<TestOrder*>(data);
    order->fired = true;
}

TEST(TimersTest, AddTimer) {
    TimersMgr mgr;
    TestOrder order;
    int timerId = mgr.addTimer(50, testCallback, &order, TimerType::PRICE_CHECK);
    EXPECT_EQ(timerId, 1);
    EXPECT_FALSE(order.fired);
}

TEST(TimersTest, StopTimer) {
    TimersMgr mgr;
    TestOrder order;
    int timerId = mgr.addTimer(50, testCallback, &order, TimerType::PRICE_CHECK);
    mgr.stopTimer(timerId);
    mgr.checkTimers();
    EXPECT_FALSE(order.fired);
}

TEST(TimersTest, CheckTimers) {
    TimersMgr mgr;
    TestOrder order;
    mgr.addTimer(50, testCallback, &order, TimerType::PRICE_CHECK);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    mgr.checkTimers();
    EXPECT_TRUE(order.fired);
}

TEST(TimersTest, FireInCorrectOrder) {
    TimersMgr mgr;
    std::vector<int> firedOrder;
    
    auto orderCallback = [](int id, void* data) {
        std::vector<int>* firedOrder = static_cast<std::vector<int>*>(data);
        firedOrder->push_back(id);
    };
    
    int id1 = mgr.addTimer(150, orderCallback, &firedOrder, TimerType::PRICE_CHECK);
    int id2 = mgr.addTimer(50, orderCallback, &firedOrder, TimerType::PRICE_CHECK);
    int id3 = mgr.addTimer(100, orderCallback, &firedOrder, TimerType::PRICE_CHECK);
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    mgr.checkTimers();
    
    EXPECT_EQ(firedOrder.size(), 3);
    EXPECT_EQ(firedOrder[0], id2);  // 50ms
    EXPECT_EQ(firedOrder[1], id3);  // 100ms
    EXPECT_EQ(firedOrder[2], id1);  // 150ms
}

TEST(TimersTest, CancelTimerBeforeFire) {
    TimersMgr mgr;
    std::vector<int> firedOrder;
    
    auto orderCallback = [](int id, void* data) {
        std::vector<int>* firedOrder = static_cast<std::vector<int>*>(data);
        firedOrder->push_back(id);
    };
    
    int id1 = mgr.addTimer(100, orderCallback, &firedOrder, TimerType::PRICE_CHECK);
    int id2 = mgr.addTimer(50, orderCallback, &firedOrder, TimerType::PRICE_CHECK);
    int id3 = mgr.addTimer(150, orderCallback, &firedOrder, TimerType::PRICE_CHECK);
    
    mgr.stopTimer(id2);  // Cancel the 50ms timer
    
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    mgr.checkTimers();
    
    EXPECT_EQ(firedOrder.size(), 2);
    EXPECT_EQ(firedOrder[0], id1);  // 100ms
    EXPECT_EQ(firedOrder[1], id3);  // 150ms
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
