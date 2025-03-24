#pragma once
#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <memory>
#include <atomic>
#include <thread>
#include "tracer.h"

enum class EventType {
    TIMER,
    ORDER_BOOK_UPDATE,
    ORDER_STATUS_CHANGE,
    BALANCE_UPDATE,
    SHUTDOWN_REQUEST
};

struct Event {
    EventType type;
    std::chrono::system_clock::time_point timestamp;
    std::function<void()> handler;
};

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Core functionality
    void start();
    void stop();
    void postEvent(EventType type, std::function<void()> handler);

    // Status
    bool isRunning() const { return running; }
    std::chrono::system_clock::time_point getStartTime() const { return startTime; }

private:
    void processEvents();
    void eventLoop();

    std::atomic<bool> running{false};
    std::chrono::system_clock::time_point startTime;
    
    std::queue<Event> eventQueue;
    std::mutex queueMutex;
    std::condition_variable queueCV;
    
    std::thread eventThread;
}; 