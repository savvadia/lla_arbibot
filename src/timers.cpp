#include <sstream>
#include <iomanip>
#include <thread>
#include <iostream>

#include "timers.h"
#include "tracer.h"
#include "config.h"

// Define TRACE macro for TimersManager
#define TRACE(_timer, ...) TRACE_OBJ("INFO ", &_timer, TraceInstance::TIMER, ExchangeId::UNKNOWN, __VA_ARGS__)
#define ERROR(_timer, ...) ERROR_OBJ(&_timer, TraceInstance::TIMER, ExchangeId::UNKNOWN, __VA_ARGS__)
// Initialize static member
int TimersManager::nextId = 1;

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

int TimersManager::addTimer(int intervalMs, TimerCallback callback, void* data, TimerType type, bool isPeriodic) {
    auto timeToFire = std::chrono::steady_clock::now() + std::chrono::milliseconds(intervalMs);
    return addTimer(timeToFire, intervalMs, callback, data, type, isPeriodic);
}

int TimersManager::addTimer(std::chrono::steady_clock::time_point timeToFire, int intervalMs, TimerCallback callback, void* data, TimerType type, bool isPeriodic) {
    Timer timer;
    timer.id = nextId++;
    timer.interval = intervalMs;
    timer.timeToFire = timeToFire;
    timer.callback = callback;
    timer.data = data;
    timer.type = type;
    timer.isPeriodic = isPeriodic;
    TRACE(timer, "Added with interval ", intervalMs, "ms");

    {
        MUTEX_LOCK(timerMutex);
        auto it = timers.insert({timer.timeToFire, timer});
        timerIds[timer.id] = it;
    }
    return timer.id;
}

void TimersManager::stopTimer(int id) {
    MUTEX_LOCK(timerMutex);
    auto it = timerIds.find(id);
    if (it != timerIds.end()) {
        TRACE(it->second->second, "stopped");
        timers.erase(it->second);
        timerIds.erase(it);
    }
}

void TimersManager::checkTimers() {
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
        auto delay_micros = std::chrono::duration_cast<std::chrono::microseconds>(now - timer.timeToFire).count();
        TRACE(timer, "fired (delay: ", delay_micros, " us)");

        if (delay_micros > Config::MAX_TIMER_DELAY_TRACE_MS * 1000) {
            ERROR(timer, "Timer fired with long delay: ", delay_micros, " us");
        }
        
        if (timer.isPeriodic) {
            // Calculate next fire time based on the previous fire time
            auto nextFireTime = timer.timeToFire + std::chrono::milliseconds(timer.interval);
            addTimer(nextFireTime, timer.interval, timer.callback, timer.data, timer.type, true);
        }
        
        auto start = std::chrono::steady_clock::now();
        timer.callback(timer.id, timer.data);
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
        
        if (duration > Config::MAX_CALLBACK_EXECUTION_TIME_MS * 1000) {
            TRACE(timer, "callback took ", duration, "us (max: ", Config::MAX_CALLBACK_EXECUTION_TIME_MS * 1000, "us)");
        } else {
            TRACE(timer, "callback took ", duration, "us (max: ", Config::MAX_CALLBACK_EXECUTION_TIME_MS * 1000, "us)");
        }
    }
}
