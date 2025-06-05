#ifndef TIMERS_H
#define TIMERS_H

#include <chrono>
#include <string>
#include <map>
#include <unordered_map>
#include <mutex>
#include <ostream>
#include "tracer.h"

extern TimersManager& timersManager;

typedef void (*TimerCallback)(int id, void* data);
void sleep_ms(int ms);

// Timer types for fast comparison
enum class TimerType {
    UNKNOWN = 0,
    PRICE_CHECK,
    RESET_BEST_SEEN_OPPORTUNITY,
    RESET_COUNTABLE_TRACES,
    EXCHANGE_CHECK_SNAPSHOT_VALIDITY,
    EXCHANGE_PING,
    COUNT
};

// Convert timer type to string (only used in traces)
inline const char* timerTypeToString(TimerType type) {
    switch (type) {
        case TimerType::UNKNOWN: return "UNKNOWN";
        case TimerType::PRICE_CHECK: return "PRICE_CHECK";
        case TimerType::RESET_BEST_SEEN_OPPORTUNITY: return "RESET_BEST_SEEN_OPPORTUNITY";
        case TimerType::RESET_COUNTABLE_TRACES: return "RESET_COUNTABLE_TRACES";
        case TimerType::EXCHANGE_CHECK_SNAPSHOT_VALIDITY: return "EXCHANGE_CHECK_SNAPSHOT_VALIDITY";
        case TimerType::EXCHANGE_PING: return "EXCHANGE_PING";
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
    bool isPeriodic;
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

class TimersManager {
public:
    TimersManager() = default;
    
    // Thread-safe timer management:
    // - addTimer: Can be called from any thread
    // - stopTimer: Can be called from any thread
    // - checkTimers: Must be called only from the main thread
    int addTimer(int intervalMs, TimerCallback callback, void* data, TimerType type, bool isPeriodic = false);
    int addTimer(std::chrono::steady_clock::time_point timeToFire, int intervalMs, TimerCallback callback, void* data, TimerType type, bool isPeriodic = false);
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
