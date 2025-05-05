#pragma once
#ifndef TRACER_TIMER_H
#define TRACER_TIMER_H

#include "tracer.h"
#include "timers.h"
#include "config.h"

// Callback for resetting countable traces
inline void resetCountableTracesTimerCallback(int id, void* data) {
    TimersMgr* timersMgr = static_cast<TimersMgr*>(data);
    if (timersMgr) {
        timersMgr->addTimer(Config::RESET_INTERVAL_MS, resetCountableTracesTimerCallback, timersMgr, TimerType::PRICE_CHECK);
    }
    FastTraceLogger::resetCountableTraces();
}

// Initialize the timer for resetting countable traces
inline void initResetCountableTracesTimer(TimersMgr& timersMgr) {
    TRACE_BASE(TraceInstance::TIMER, ExchangeId::UNKNOWN, "Initializing reset countable traces timer");
    resetCountableTracesTimerCallback(0, &timersMgr);
}


#endif // TRACER_TIMER_H 