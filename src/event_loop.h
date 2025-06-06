#pragma once

#include <functional>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <atomic>
#include <thread>
#include "types.h"

enum class EventType {
    // Timer events
    TIMER,

    // Market data events
    MARKET_DATA,
    ORDER_BOOK_UPDATE,
    
    // Exchange-specific events
    WEBSOCKET_MESSAGE,
    BINANCE_UPDATED,
    KRAKEN_UPDATED,
    EXCHANGE_UPDATE,
    
    // Order events
    ORDER_STATUS_CHANGE,
    
    // Balance events
    BALANCE_UPDATE,
    
    // System events
    SYSTEM_EVENT,
    SHUTDOWN_REQUEST
};

struct Event {
    EventType type;
    std::function<void()> callback;
    std::chrono::steady_clock::time_point timestamp;

    Event(EventType t, std::function<void()> cb, std::chrono::steady_clock::time_point ts)
        : type(t), callback(cb), timestamp(ts) {}
};

class EventLoop {
public:
    EventLoop();
    ~EventLoop();

    // Core functionality
    void start();
    void stop();
    void postEvent(EventType type, std::function<void()> callback);

    // Post an order book update event
    void postOrderBookUpdate(std::function<void()> callback);

    // Post an exchange update event
    void postExchangeUpdate(ExchangeId exchange);

    // Status
    bool isRunning() const { return m_running; }
    std::chrono::system_clock::time_point getStartTime() const { return startTime; }

private:
    void processEvents();
    void eventLoop();

    std::atomic<bool> m_running{false};
    std::chrono::system_clock::time_point startTime;
    
    std::queue<Event> m_eventQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondition;
    
    std::thread eventThread;

    void processEvent(const Event& event);
}; 