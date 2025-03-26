#pragma once

#include <functional>
#include <chrono>
#include "tracer.h"

class Timer : public Traceable {
private:
    std::function<void()> callback;
    std::chrono::steady_clock::time_point nextRun;
    std::chrono::milliseconds interval;
    bool isPeriodic;

public:
    Timer(std::function<void()> cb, std::chrono::milliseconds interval, bool periodic = false);
    void update();
    bool shouldRun() const;
    void run();
    void reset();
    bool isOneShot() const { return !isPeriodic; }

protected:
    void trace(std::ostream& os) const override {
        os << "Timer[interval=" << interval.count() << "ms, periodic=" << isPeriodic 
           << ", nextRun=" << std::chrono::duration_cast<std::chrono::milliseconds>(
               nextRun - std::chrono::steady_clock::now()).count() << "ms]";
    }
}; 