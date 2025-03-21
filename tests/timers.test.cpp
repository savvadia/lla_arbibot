#include <chrono>
#include <thread>
#include "timers.h"
#include <gtest/gtest.h>

using namespace std;

void testCallback(int id, void* data) {
    cout << "Timer " << id << " fired" << endl;
    std::vector<int>* order = static_cast<std::vector<int>*>(data);
    order->push_back(id);
}

void sleep_in_test(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

TEST(TimersMgrTest, AddTimer) {
    TimersMgr mgr;
    std::vector<int> order;
    int timerId = mgr.addTimer(10, testCallback, &order);
    EXPECT_GT(timerId, 0);
}

TEST(TimersMgrTest, StopTimer) {
    TimersMgr mgr;
    std::vector<int> order;
    int timerId = mgr.addTimer(10, testCallback, &order);
    mgr.stopTimer(timerId);
    sleep_in_test(20);
    mgr.checkTimers();
    EXPECT_TRUE(order.empty());
}

TEST(TimersMgrTest, CheckTimers) {
    TimersMgr mgr;
    std::vector<int> order;
    mgr.addTimer(10, testCallback, &order);
    sleep_in_test(20);
    mgr.checkTimers();
    EXPECT_EQ(order.size(), 1);
}

TEST(TimersMgrTest, FireInCorrectOrder) {
    TimersMgr mgr;
    std::vector<int> order;
    int id1 = mgr.addTimer(30, testCallback, &order);
    int id2 = mgr.addTimer(10, testCallback, &order);
    int id3 = mgr.addTimer(20, testCallback, &order);
    sleep_in_test(40);
    mgr.checkTimers();
    EXPECT_EQ(order, (std::vector<int>{id2, id3, id1}));
}

TEST(TimersMgrTest, CancelTimerBeforeFire) {
    TimersMgr mgr;
    std::vector<int> order;
    int id1 = mgr.addTimer(20, testCallback, &order);
    int id2 = mgr.addTimer(10, testCallback, &order);
    mgr.addTimer(30, testCallback, &order);
    mgr.stopTimer(id1);
    sleep_in_test(25);
    mgr.checkTimers();
    EXPECT_EQ(order, (std::vector<int>{id2}));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
