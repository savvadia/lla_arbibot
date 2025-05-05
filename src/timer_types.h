#pragma once
#ifndef TIMER_TYPES_H
#define TIMER_TYPES_H

// Timer types for fast comparison
enum class TimerType {
    UNKNOWN = 0,
    BALANCE_CHECK,
    ORDER_CHECK,
    PRICE_CHECK,
    MAX
};

// Convert timer type to string
inline const char* timerTypeToString(TimerType type) {
    switch (type) {
        case TimerType::UNKNOWN: return "UNKNOWN";
        case TimerType::BALANCE_CHECK: return "BALANCE_CHECK";
        case TimerType::ORDER_CHECK: return "ORDER_CHECK";
        case TimerType::PRICE_CHECK: return "PRICE_CHECK";
        default: return "INVALID";
    }
}

#endif // TIMER_TYPES_H 