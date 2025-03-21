#include "timers.h"
#include <sstream>
#include <iomanip>

#include "tracer.h"

#define TRACE(_timer, ...) TRACE_INST_TIMER(_timer, __VA_ARGS__)
TimersMgr::TimersMgr() {}

// Static method to format any time_point to hh:mm:ss.sss
std::string Timer::formatTime(std::chrono::steady_clock::time_point time) {
    auto diff = time - std::chrono::steady_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff).count();
    auto s = ms / 1000;
    ms = ms % 1000;
    auto m = s / 60;
    s = s % 60;
    auto h = m / 60;
    m = m % 60;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << h << ":" 
        << std::setfill('0') << std::setw(2) << m << ":" 
        << std::setfill('0') << std::setw(2) << s << "." 
        << std::setfill('0') << std::setw(3) << ms;
    return oss.str();
}

// Non-static method to format the fire time of the timer (uses the timeToFire member)
std::string Timer::formatFireTime() const {
    return formatTime(timeToFire);
}

bool Timer::isExpired(std::chrono::steady_clock::time_point now) const {
    return timeToFire <= now;
}

int TimersMgr::addTimer(int intervalMs, TimerCallback callback, void* data) {
    Timer timer;
    timer.id = nextId++;
    timer.interval = intervalMs;
    timer.timeToFire = std::chrono::steady_clock::now() + std::chrono::milliseconds(intervalMs);
    timer.callback = callback;
    timer.data = data;

    TRACE(timer, "added with intervalMs %d", intervalMs);

    auto it = timers.insert({timer.timeToFire, timer}).first;
    timerIds[timer.id] = it;
    return timer.id;
}

void TimersMgr::stopTimer(int id) {
    auto it = timerIds.find(id);
    if (it != timerIds.end()) {
        TRACE(it->second->second, "stopped");
        timers.erase(it->second);
        timerIds.erase(it);
    } else {
#ifdef DEBUG
        std::cout << "Timer with id " << id << " not found. nextId=" << nextId << std::endl;
#endif
    }
}

void TimersMgr::checkTimers() {
    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    for (auto it = timers.begin(); it != timers.end(); ) {
        auto& timer = it->second;
        if (timer.isExpired(now)) {
            TRACE(timer, "fired");
            timer.callback(timer.id, timer.data);
            timerIds.erase(timer.id);
            it = timers.erase(it);
        } else {
            TRACE(timer, "not fired, now:%s", Timer::formatTime(now).c_str());
            ++it;
        }
    }
}
