#ifndef TIMERS_H
#define TIMERS_H

#include <chrono>
#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <ostream>
#include "tracer.h"

typedef void (*TimerCallback)(int id, void* data);
void sleep_ms(int ms);

// Timer types for fast comparison
enum class TimerType {
    UNKNOWN = 0,
    BALANCE_CHECK,
    ORDER_CHECK,
    PRICE_CHECK,
    RESET_BEST_SEEN_OPPORTUNITY,
    EXCHANGE_CHECK_SNAPSHOT_VALIDITY,
    MAX
};

// Convert timer type to string (only used in traces)
inline const char* timerTypeToString(TimerType type) {
    switch (type) {
        case TimerType::UNKNOWN: return "UNKNOWN";
        case TimerType::BALANCE_CHECK: return "BALANCE_CHECK";
        case TimerType::ORDER_CHECK: return "ORDER_CHECK";
        case TimerType::PRICE_CHECK: return "PRICE_CHECK";
        default: return "INVALID";
    }
}

// Timer callback requirements:
// 1. Must complete within 10ms
// 2. Must not block
// 3. For longer operations, post an event to the event loop
// 4. Must handle its own thread safety if needed (e.g. mutex for shared data)
struct Timer : public Traceable {
    int id;
    int interval;
    std::chrono::steady_clock::time_point timeToFire;
    TimerCallback callback;
    void* data;
    TimerType type;

    // Static method to format any time_point to hh:mm:ss.sss (used in traces)
    static std::string formatTime(std::chrono::steady_clock::time_point time);

    // Format the time to fire (uses the timeToFire member)
    std::string formatFireTime() const;

    // Check if the timer has expired
    bool isExpired(std::chrono::steady_clock::time_point now) const;

    void trace(std::ostream& os) const override {
        os << id << ' ' << formatFireTime() << " " << timerTypeToString(type);
    }
};

class TimersMgr {
public:
    static constexpr int DEFAULT_CHECK_INTERVAL_MS = 20;   // Check timers every 20ms
    static constexpr int MAX_CALLBACK_TIME_MS = 10;        // Callbacks must complete within 10ms

    TimersMgr() = default;
    
    // Thread-safe timer management:
    // - addTimer: Can be called from any thread
    // - stopTimer: Can be called from any thread
    // - checkTimers: Must be called only from the main thread
    int addTimer(int intervalMs, TimerCallback callback, void* data, TimerType type);
    void stopTimer(int id);
    void checkTimers();  // Called directly from main

private:
    // Thread-safe timer storage
    std::multimap<std::chrono::steady_clock::time_point, Timer> timers;
    std::unordered_map<int, std::multimap<std::chrono::steady_clock::time_point, Timer>::iterator> timerIds;
    static int nextId;  // Static member variable
    std::mutex timerMutex;  // Protects timer operations from multiple threads
};

#endif // TIMERS_H
