#include "timers.h"
#include "event_loop.h"
#include <sstream>
#include <iomanip>
#include <thread>

#include "tracer.h"
#include <iostream>

#ifdef DISABLE_TRACES
#define TRACE(_timer, ...) ((void)0)
#else
#define TRACE(_timer, ...) TRACE_INST_TIMER(_timer, __VA_ARGS__)
#endif

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

    TRACE(timer, "added with intervalMs %d", intervalMs);

    {
        std::lock_guard<std::mutex> lock(timerMutex);
        auto it = timers.insert({timer.timeToFire, timer});
        timerIds[timer.id] = it;
    }
    return timer.id;
}

void TimersMgr::stopTimer(int id) {
    std::lock_guard<std::mutex> lock(timerMutex);
    auto it = timerIds.find(id);
    if (it != timerIds.end()) {
        TRACE(it->second->second, "stopped");
        timers.erase(it->second);
        timerIds.erase(it);
    }
}

void TimersMgr::checkTimers() {
    auto now = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(timerMutex);
    
    auto it = timers.begin();
    while (it != timers.end() && it->first <= now) {
        auto& timer = it->second;
        auto delay = std::chrono::duration_cast<std::chrono::microseconds>(now - timer.timeToFire).count();
        TRACE(timer, "fired (delay: %lld us)", delay);
        
        auto start = std::chrono::steady_clock::now();
        timer.callback(timer.id, timer.data);
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (duration > MAX_CALLBACK_TIME_MS * 1000) {
            TRACE(timer, "Timer callback took %lldus (max: %dus)", duration, MAX_CALLBACK_TIME_MS * 1000);
        } else {
            TRACE(timer, "Timer '%s' callback took %lldus (max: %dus)", 
                timerTypeToString(timer.type), duration, MAX_CALLBACK_TIME_MS * 1000);
        }
        it = timers.erase(it);
    }
}
