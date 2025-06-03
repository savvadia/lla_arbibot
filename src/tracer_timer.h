#pragma once
#ifndef TRACER_TIMER_H
#define TRACER_TIMER_H

#include "tracer.h"
#include "timers.h"
#include "config.h"

// Callback for resetting countable traces
inline void resetCountableTracesTimerCallback(int id, void* data) {
    FastTraceLogger::resetCountableTraces();
}

// Initialize the timer for resetting countable traces
inline void initResetCountableTracesTimer(TimersMgr& timersMgr) {
    TRACE_BASE(TraceInstance::TIMER, ExchangeId::UNKNOWN, "Initializing reset countable traces timer");
    timersMgr.addTimer(Config::COUNTABLE_TRACES_RESET_INTERVAL_MS,
        resetCountableTracesTimerCallback, nullptr, TimerType::RESET_COUNTABLE_TRACES, true);
}


#endif // TRACER_TIMER_H 