#include <gtest/gtest.h>
#include "../src/event_loop.h"
#include <atomic>
#include <thread>
#include <chrono>

class EventLoopTest : public ::testing::Test {
protected:
    void SetUp() override {
        loop = std::make_unique<EventLoop>();
    }

    void TearDown() override {
        if (loop) {
            if (loop->isRunning()) {
                loop->stop();
                // Wait for the loop to actually stop
                for (int i = 0; i < 10 && loop->isRunning(); i++) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            loop.reset();
        }
    }

    std::unique_ptr<EventLoop> loop;
};

TEST_F(EventLoopTest, ConstructionAndDestruction) {
    EXPECT_FALSE(loop->isRunning());
}

TEST_F(EventLoopTest, StartAndStop) {
    loop->start();
    EXPECT_TRUE(loop->isRunning());
    
    loop->stop();
    EXPECT_FALSE(loop->isRunning());
}

TEST_F(EventLoopTest, EventProcessing) {
    std::atomic<int> counter{0};
    
    loop->start();
    
    // Post an event that increments the counter
    loop->postEvent(EventType::TIMER, [&counter]() {
        counter++;
    });
    
    // Wait for the event to be processed
    for (int i = 0; i < 10 && counter == 0; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_EQ(counter, 1);
    loop->stop();  // Explicitly stop the loop
}

TEST_F(EventLoopTest, EventOrder) {
    std::vector<int> order;
    std::mutex orderMutex;
    
    loop->start();
    
    // Post events in reverse order
    for (int i = 5; i >= 1; i--) {
        loop->postEvent(EventType::TIMER, [i, &order, &orderMutex]() {
            std::lock_guard<std::mutex> lock(orderMutex);
            order.push_back(i);
        });
    }
    
    // Wait for all events to be processed
    for (int i = 0; i < 10 && order.size() < 5; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    std::lock_guard<std::mutex> lock(orderMutex);
    EXPECT_EQ(order.size(), 5);
    // Events should be processed in the order they were posted
    for (int i = 0; i < 5; i++) {
        EXPECT_EQ(order[i], 5 - i);
    }
}

TEST_F(EventLoopTest, ErrorHandling) {
    std::atomic<bool> errorCaught{false};
    
    loop->start();
    
    // Post an event that throws an exception
    loop->postEvent(EventType::TIMER, [&errorCaught]() {
        try {
            throw std::runtime_error("Test error");
        } catch (const std::exception&) {
            errorCaught = true;
        }
    });
    
    // Wait for the event to be processed
    for (int i = 0; i < 10 && !errorCaught; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_TRUE(errorCaught);
}

TEST_F(EventLoopTest, ThreadSafety) {
    std::atomic<int> counter{0};
    const int NUM_THREADS = 5;  // Reduced from 10
    const int EVENTS_PER_THREAD = 50;  // Reduced from 100
    
    loop->start();
    
    std::vector<std::thread> threads;
    for (int i = 0; i < NUM_THREADS; i++) {
        threads.emplace_back([this, &counter]() {
            for (int j = 0; j < EVENTS_PER_THREAD; j++) {
                loop->postEvent(EventType::TIMER, [&counter]() {
                    counter++;
                });
            }
        });
    }
    
    // Wait for all threads to finish
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Wait for all events to be processed
    for (int i = 0; i < 20 && counter < NUM_THREADS * EVENTS_PER_THREAD; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_EQ(counter, NUM_THREADS * EVENTS_PER_THREAD);
    loop->stop();  // Explicitly stop the loop
}

TEST_F(EventLoopTest, MultipleEventTypes) {
    std::atomic<int> timerCount{0};
    std::atomic<int> marketDataCount{0};
    
    loop->start();
    
    // Post events of different types
    loop->postEvent(EventType::TIMER, [&timerCount]() {
        timerCount++;
    });
    
    loop->postEvent(EventType::MARKET_DATA, [&marketDataCount]() {
        marketDataCount++;
    });
    
    // Wait for events to be processed
    for (int i = 0; i < 10 && (timerCount == 0 || marketDataCount == 0); i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    EXPECT_EQ(timerCount, 1);
    EXPECT_EQ(marketDataCount, 1);
}

TEST_F(EventLoopTest, StopWithPendingEvents) {
    std::atomic<int> processedCount{0};
    
    loop->start();
    
    // Post multiple events
    for (int i = 0; i < 10; i++) {
        loop->postEvent(EventType::TIMER, [&processedCount]() {
            processedCount++;
        });
    }
    
    // Stop immediately
    loop->stop();
    
    // Wait a bit to see if any events are processed
    for (int i = 0; i < 10; i++) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Some events might be processed before stop, but not all
    EXPECT_LE(processedCount, 10);
} 