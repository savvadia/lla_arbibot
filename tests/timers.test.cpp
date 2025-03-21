#include "timers.h"
#include <gtest/gtest.h>

using namespace std;

void timerCallback(int id, void* data) {
    cout << "Timer " << id << " fired" << endl;
    std::vector<int>* order = static_cast<std::vector<int>*>(data);
    order->push_back(id);
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
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    mgr.checkTimers();
    EXPECT_TRUE(order.empty());
}

TEST(TimersMgrTest, CheckTimers) {
    TimersMgr mgr;
    std::vector<int> order;
    mgr.addTimer(10, testCallback, &order);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    mgr.checkTimers();
    EXPECT_EQ(order.size(), 1);
}

TEST(TimersMgrTest, FireInCorrectOrder) {
    TimersMgr mgr;
    std::vector<int> order;
    mgr.addTimer(30, testCallback, &order); // ID 1
    mgr.addTimer(10, testCallback, &order); // ID 2
    mgr.addTimer(20, testCallback, &order); // ID 3
    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    mgr.checkTimers();
    EXPECT_EQ(order, (std::vector<int>{2, 3, 1}));
}

TEST(TimersMgrTest, CancelTimerBeforeFire) {
    TimersMgr mgr;
    std::vector<int> order;
    int id1 = mgr.addTimer(20, testCallback, &order); // ID 1
    int id2 = mgr.addTimer(10, testCallback, &order); // ID 2
    int id3 = mgr.addTimer(30, testCallback, &order); // ID 3
    mgr.stopTimer(id1);
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    mgr.checkTimers();
    EXPECT_EQ(order, (std::vector<int>{2}));
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
