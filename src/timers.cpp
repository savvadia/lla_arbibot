#include "timers.h"
#include <sstream>
#include <iomanip>
#include <thread>

#include "tracer.h"
#include <iostream>

// Define TRACE macro for TimersMgr
#define TRACE(_timer, ...) TRACE_OBJ("INFO ", &_timer, TraceInstance::TIMER, ExchangeId::UNKNOWN, __VA_ARGS__)

// Initialize static member
int TimersMgr::nextId = 1;

void sleep_ms(int ms) {
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Optimized time formatting function - preallocate stringstream
std::string Timer::formatTime(std::chrono::steady_clock::time_point time) {
    static thread_local std::ostringstream oss;  // Thread-local for thread safety
    oss.str("");  // Clear the stream
    oss.clear();  // Clear any error flags
    
    auto diff = time - std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    auto s = ms / 1000;
    ms = ms % 1000;
    auto m = s / 60;
    s = s % 60;
    auto h = m / 60;
    m = m % 60;

    oss << std::setfill('0') << std::setw(2) << h << ":" 
        << std::setfill('0') << std::setw(2) << m << ":" 
        << std::setfill('0') << std::setw(2) << s << "." 
        << std::setfill('0') << std::setw(3) << ms;
    return oss.str();
}

std::string Timer::formatFireTime() const {
    return formatTime(timeToFire);
}

bool Timer::isExpired(std::chrono::steady_clock::time_point now) const {
    return timeToFire <= now;
}

int TimersMgr::addTimer(int intervalMs, TimerCallback callback, void* data, TimerType type) {
    Timer timer;
    timer.id = nextId++;
    timer.interval = intervalMs;
    timer.timeToFire = std::chrono::steady_clock::now() + std::chrono::milliseconds(intervalMs);
    timer.callback = callback;
    timer.data = data;
    timer.type = type;

    TRACE(timer, "Added with interval ", intervalMs, "ms");

    {
        MUTEX_LOCK(timerMutex);
        auto it = timers.insert({timer.timeToFire, timer});
        timerIds[timer.id] = it;
    }
    return timer.id;
}

void TimersMgr::stopTimer(int id) {
    MUTEX_LOCK(timerMutex);
    auto it = timerIds.find(id);
    if (it != timerIds.end()) {
        TRACE(it->second->second, "stopped");
        timers.erase(it->second);
        timerIds.erase(it);
    }
}

void TimersMgr::checkTimers() {
    auto now = std::chrono::steady_clock::now();
    std::vector<Timer> timersToExecute;
    
    // First, collect timers that need to be executed
    {
        MUTEX_LOCK(timerMutex);
        auto it = timers.begin();
        while (it != timers.end() && it->first <= now) {
            timersToExecute.push_back(it->second);
            // Remove from both maps
            timerIds.erase(it->second.id);
            it = timers.erase(it);
        }
    }
    
    // Now execute callbacks without holding the mutex
    for (const auto& timer : timersToExecute) {
        auto delay = std::chrono::duration_cast<std::chrono::microseconds>(now - timer.timeToFire).count();
        TRACE(timer, "fired (delay: ", delay, " us)");
        
        auto start = std::chrono::steady_clock::now();
        timer.callback(timer.id, timer.data);
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (duration > MAX_CALLBACK_TIME_MS * 1000) {
            TRACE(timer, "callback took ", duration, "us (max: ", MAX_CALLBACK_TIME_MS * 1000, "us)");
        } else {
            TRACE(timer, "callback took ", duration, "us (max: ", MAX_CALLBACK_TIME_MS * 1000, "us)");
        }
    }
}
