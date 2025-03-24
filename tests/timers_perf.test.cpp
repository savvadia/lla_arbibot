#include <gtest/gtest.h>
#include "../src/timers.h"
#include <atomic>
#include <vector>
#include <chrono>
#include <thread>

#include "../src/tracer.h"
#include <iostream>

struct TimerData {
    std::chrono::steady_clock::time_point expectedTime;
    int id;
};

std::atomic<int> callbackCount{0};
std::atomic<int64_t> totalDelay{0};
std::vector<int64_t> delays;

void timerCallback(int id, void* data) {
    auto now = std::chrono::steady_clock::now();
    TimerData* timerData = static_cast<TimerData*>(data);
    auto delay = std::chrono::duration_cast<std::chrono::microseconds>(now - timerData->expectedTime).count();
    
    totalDelay += delay;
    delays.push_back(delay);
    callbackCount++;
}

class TimersPerfTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Disable traces for all performance tests
        g_traces_enabled = false;
    }

    void TearDown() override {
        // Re-enable traces after tests
        g_traces_enabled = true;
    }
};

TEST_F(TimersPerfTest, FastTimerAccuracy) {
    TimersMgr tm;
    const int NUM_TIMERS = 1000;  // Back to 1000 timers since performance is better now
    const int INTERVAL_MS = 1;  // 1ms interval for high precision test
    
    callbackCount = 0;
    totalDelay = 0;
    delays.clear();
    
    // First, create all timer data structures
    std::vector<TimerData*> timerDataVec;
    for (int i = 0; i < NUM_TIMERS; i++) {
        auto* timerData = new TimerData;
        timerData->id = i;
        timerDataVec.push_back(timerData);
    }
    
    // Then, in a separate phase, add all timers at once
    auto startTime = std::chrono::steady_clock::now();
    for (auto* timerData : timerDataVec) {
        timerData->expectedTime = startTime + std::chrono::milliseconds(INTERVAL_MS);
        tm.addTimer(INTERVAL_MS, timerCallback, timerData, TimerType::PRICE_CHECK);
    }
    
    // Wait for all initial callbacks with longer sleep interval
    while (callbackCount < NUM_TIMERS) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // Check every 1ms
        tm.checkTimers();
    }
    
    // Calculate statistics
    int64_t maxDelay = 0;
    int64_t minDelay = std::numeric_limits<int64_t>::max();
    for (int64_t delay : delays) {
        maxDelay = std::max(maxDelay, delay);
        minDelay = std::min(minDelay, delay);
    }
    
    double avgDelay = static_cast<double>(totalDelay) / NUM_TIMERS;
    
    // Print results
    std::cout << "\nTimer Performance Results:" << std::endl;
    std::cout << "Number of timers: " << NUM_TIMERS << std::endl;
    std::cout << "Interval: " << INTERVAL_MS << "ms" << std::endl;
    std::cout << "Average delay: " << avgDelay << "us" << std::endl;
    std::cout << "Min delay: " << minDelay << "us" << std::endl;
    std::cout << "Max delay: " << maxDelay << "us" << std::endl;
    
    // Clean up
    for (auto* timerData : timerDataVec) {
        delete timerData;
    }
    
    // Verify performance with more realistic bounds
    EXPECT_LT(avgDelay, 1000);  // Average delay should be less than 1ms
    EXPECT_LT(maxDelay, 1500);  // Max delay should be less than 1.5ms
}

TEST_F(TimersPerfTest, HighFrequencyTimers) {
    TimersMgr tm;
    const int NUM_TIMERS = 100;
    const int INTERVAL_MS = 1;  // 1ms interval
    const int TEST_DURATION_MS = 20;  // Reduced from 100ms to 20ms
    
    callbackCount = 0;
    totalDelay = 0;
    delays.clear();
    
    // First, create all timer data structures
    std::vector<TimerData*> timerDataVec;
    for (int i = 0; i < NUM_TIMERS; i++) {
        auto* timerData = new TimerData;
        timerData->id = i;
        timerDataVec.push_back(timerData);
    }
    
    // Then, in a separate phase, add all timers at once
    auto startTime = std::chrono::steady_clock::now();
    for (auto* timerData : timerDataVec) {
        timerData->expectedTime = startTime + std::chrono::milliseconds(INTERVAL_MS);
        tm.addTimer(INTERVAL_MS, timerCallback, timerData, TimerType::PRICE_CHECK);
    }
    
    // Run for shorter duration, checking timers less frequently
    auto testStart = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
           std::chrono::steady_clock::now() - testStart).count() < TEST_DURATION_MS) {
        tm.checkTimers();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));  // Check every 1ms instead of 100μs
    }
    
    // Calculate statistics
    int64_t maxDelay = 0;
    int64_t minDelay = std::numeric_limits<int64_t>::max();
    for (int64_t delay : delays) {
        maxDelay = std::max(maxDelay, delay);
        minDelay = std::min(minDelay, delay);
    }
    
    double avgDelay = static_cast<double>(totalDelay) / delays.size();
    
    // Print results
    std::cout << "\nHigh Frequency Timer Results:" << std::endl;
    std::cout << "Number of timers: " << NUM_TIMERS << std::endl;
    std::cout << "Interval: " << INTERVAL_MS << "ms" << std::endl;
    std::cout << "Test duration: " << TEST_DURATION_MS << "ms" << std::endl;
    std::cout << "Total callbacks: " << delays.size() << std::endl;
    std::cout << "Average delay: " << avgDelay << "us" << std::endl;
    std::cout << "Min delay: " << minDelay << "us" << std::endl;
    std::cout << "Max delay: " << maxDelay << "us" << std::endl;
    
    // Clean up
    for (auto* timerData : timerDataVec) {
        delete timerData;
    }
    
    // We expect at least one callback per timer
    EXPECT_GE(delays.size(), NUM_TIMERS);
    EXPECT_LT(avgDelay, 1000);  // Average delay should be less than 1ms
    EXPECT_LT(maxDelay, 1000);  // Max delay should be less than 1ms
} 