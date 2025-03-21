#ifndef TIMER_H
#define TIMER_H

#include <chrono>
#include <string>
#include <map>
#include <unordered_map>

using TimerCallback = void (*)(int id, void* data);

class Timer {
public:
    int id;
    int interval;
    std::chrono::steady_clock::time_point timeToFire;
    TimerCallback callback;
    void* data;

    // Static method to format any time_point to hh:mm:ss.sss
    static std::string formatTime(std::chrono::steady_clock::time_point time);

    // Non-static method to format the time to fire (uses the timeToFire member)
    std::string formatFireTime() const;

    // Check if the timer has expired
    bool isExpired(std::chrono::steady_clock::time_point now) const;
};

class TimersMgr {
private:
    std::map<std::chrono::steady_clock::time_point, Timer> timers;
    std::unordered_map<int, std::map<std::chrono::steady_clock::time_point, Timer>::iterator> timerIds;
    int nextId = 1;

public:
    TimersMgr();
    int addTimer(int intervalMs, TimerCallback callback, void* data);
    void stopTimer(int id);
    void checkTimers();
};

#endif // TIMER_H
