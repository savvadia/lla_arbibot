#ifndef TIMER_H
#define TIMER_H

#include <iostream>
#include <map>
#include <unordered_map>
#include <ctime>

using TimerCallback = void (*)(int id, void* data);

struct Timer {
    int id;
    int interval;
    time_t timeToFire;
    TimerCallback callback;
    void* data;
};

class TimersMgr {
private:
    std::map<time_t, Timer> timers;
    std::unordered_map<int, std::map<time_t, Timer>::iterator> timerIds;
    int nextId = 1;

public:
    TimersMgr();
    int addTimer(int interval, TimerCallback callback, void* data);
    void stopTimer(int id);
    void checkTimers();
};

#endif // TIMER_H


