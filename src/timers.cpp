#include "timers.h"

TimersMgr::TimersMgr() {}

int TimersMgr::addTimer(int interval, TimerCallback callback, void* data) {
    Timer timer;
    timer.id = nextId++;
    timer.interval = interval;
    timer.timeToFire = time(nullptr) + interval;
    timer.callback = callback;
    timer.data = data;

    auto it = timers.insert({timer.timeToFire, timer}).first;
    timerIds[timer.id] = it;
    return timer.id;
}

void TimersMgr::stopTimer(int id) {
    auto it = timerIds.find(id);
    if (it != timerIds.end()) {
        timers.erase(it->second);
        timerIds.erase(it);
    } else {
#ifdef DEBUG
        std::cout << "Timer with id " << id << " not found. nextId=" << nextId << std::endl;
#endif
    }
}

void TimersMgr::checkTimers() {
    time_t now = time(nullptr);
    for (auto it = timers.begin(); it != timers.end(); ) {
        if (it->first <= now) {
            it->second.callback(it->second.id, it->second.data);
            timerIds.erase(it->second.id);
            it = timers.erase(it);
        } else {
            ++it;
        }
    }
}

/*
class Timer {
    int id;
    int interval;
    time_t timeToFire;
    void (*callback)(int id, void* data);
    void* data;
};

// handles timers. Keeps track of timers to fire and calls the callback when the time is up.
// The timers are checked on request, not automatically.
class TimersMgr {
    map<time_t, Timer> timers;
    int nextId = 1;
    // map of timers: id -> map instance to be able to delete the timer from the list
    unordered_map<int, map<time_t, Timer>::iterator> timerIds;
public:
    TimersMgr() {
        
    }
    int addTimer(int interval, void (*callback)(int id, void* data), void* data) {
        Timer timer;
        timer.id = nextId++;
        timer.interval = interval;
        timer.timeToFire = time(nullptr) + interval;
        timer.callback = callback;
        timer.data = data;

        // add the timer to the map and get the iterator
        auto it = timers.insert({timer.timeToFire, timer}).first;
        timerIds[timer.id] = it;
        return timer.id;
    }
    void stopTimer(int id) {
        auto it = timerIds.find(id);
        if (it != timerIds.end()) {
            timers.erase(it->second);
            timerIds.erase(it);
        } else {
            cout << "Timer with id " << id << " not found. nextId=" << nextId << endl;
        }
    }
    void checkTimers() {
        time_t now = time(nullptr);
        // loop timers, call the callback for expired timers, erase them from both maps
        for (auto it = timers.begin(); it != timers.end(); ) {
            if (it->first <= now) {
                it->second.callback(it->second.id, it->second.data);
                timerIds.erase(it->second.id);
                it = timers.erase(it);
            } else {
                ++it;
            }
        }
    }
}

*/


