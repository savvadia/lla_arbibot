#pragma once
#ifndef TRACER_H
#define TRACER_H

#include <iostream>
#include <fstream>
#include <mutex>
#include <atomic>
#include <string_view>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>
#include "types.h"  // For ExchangeId
#include "config.h" // For COUNTABLE_TRACES_PRINT_INTERVAL
// Forward declaration of TimersMgr
class TimersMgr;

// Enum for logging types
enum class TraceInstance {
    TRACES,
    TIMER,
    BALANCE,
    EVENT_LOOP,
    EX_MGR,
    STRAT,
    ORDERBOOK,
    ORDERBOOK_MGR,
    EVENTLOOP,
    A_EXCHANGE,
    A_IO,
    A_KRAKEN,
    A_BINANCE,
    A_KUCOIN,
    MAIN,  // For main function logging
    MUTEX,
    COUNT,  // To track the number of log types
};

enum class CountableTrace {
    S_POPLAVKI_OPPORTUNITY,
    S_POPLAVKI_OPPORTUNITY_EXECUTABLE,
    A_EXCHANGE_NOT_CONNECTED,
    A_EXCHANGE_SNAPSHOT_STALE,
    A_EXCHANGE_SNAPSHOT_MISSING,
    A_KRAKEN_ORDERBOOK_UPDATE,
    A_KRAKEN_ORDERBOOK_CHECKSUM_CHECK,
    A_KRAKEN_ORDERBOOK_CHECKSUM_CHECK2,
    A_KRAKEN_ORDERBOOK_CHECKSUM_CHECK_OK,
    A_KRAKEN_CHECKSUM_MISMATCH_RESTORED,
    A_UNKNOWN_MESSAGE_RECEIVED,
    COUNT,
};


// Base macro for logging with object
#define TRACE_OBJ(_level, _obj, _type, _exchangeId, ...)                                            \
    if (FastTraceLogger::globalLoggingEnabled().load(std::memory_order_relaxed) &&  \
        FastTraceLogger::logLevels()[static_cast<int>(_type)].load(std::memory_order_relaxed) && \
        (_exchangeId == ExchangeId::UNKNOWN || FastTraceLogger::isLoggingEnabled(_exchangeId))) { \
        FastTraceLogger::log(_level, _obj, _type, _exchangeId, __FILE__, __LINE__, __VA_ARGS__); \
    }

#define TRACE_THIS(_type, _exchangeId, ...) TRACE_OBJ("INFO ", this, _type, _exchangeId, __VA_ARGS__)
#define TRACE_BASE(_type, _exchangeId, ...) TRACE_OBJ("INFO ", nullptr, _type, _exchangeId, __VA_ARGS__)

// For logging with countable trace
#define TRACE_COUNT(_type, _id, _exchangeId, ...) \
    if(FastTraceLogger::globalLoggingEnabled().load(std::memory_order_relaxed) && \
        FastTraceLogger::logLevels()[static_cast<int>(_type)].load(std::memory_order_relaxed) && \
        (_exchangeId == ExchangeId::UNKNOWN || FastTraceLogger::isLoggingEnabled(_exchangeId))) { \
        FastTraceLogger::countableLog("INFO ", this, _type, _id, _exchangeId, __FILE__, __LINE__, __VA_ARGS__); \
    }

// no checks
#define ERROR_OBJ(_obj, _type, _exchangeId, ...) FastTraceLogger::log("ERROR", _obj, _type, _exchangeId, __FILE__, __LINE__, __VA_ARGS__);
#define ERROR_THIS(_type, _exchangeId, ...) ERROR_OBJ(this, _type, _exchangeId, __VA_ARGS__)
#define ERROR_BASE(_type, _exchangeId, ...) ERROR_OBJ(nullptr, _type, _exchangeId, __VA_ARGS__)
#define ERROR_COUNT(_type, _id, _exchangeId, ...) \
    FastTraceLogger::countableLog("ERROR", this, _type, _id, _exchangeId, __FILE__, __LINE__, __VA_ARGS__);

// Debug versions (disabled by default)
#if 0
    #define DEBUG_THIS(_type, _exchangeId, ...) TRACE_OBJ("DEBUG", this, _type, _exchangeId, __VA_ARGS__)
    #define DEBUG_BASE(_type, _exchangeId, ...) TRACE_OBJ("DEBUG", nullptr, _type, _exchangeId, __VA_ARGS__)
    #define DEBUG_OBJ(_level, _obj, _type, _exchangeId, ...) TRACE_OBJ(_level, _obj, _type, _exchangeId, __VA_ARGS__)
#else
    #define DEBUG_THIS(_type, _exchangeId, ...) ((void)0)
    #define DEBUG_BASE(_type, _exchangeId, ...) ((void)0)
    #define DEBUG_OBJ(_level, _obj, _type, _exchangeId, ...) ((void)0)
#endif

#define CONCATENATE_DETAIL(x, y) x##y
#define UNIQUE_LOCK_NAME(_base, _line) CONCATENATE_DETAIL(_base, _line)
#if 0
#define MUTEX_LOCK(_mutex) \
    std::cout<<__FILE__<<":"<<__LINE__<<" locking "<<#_mutex<<std::endl; \
    std::lock_guard<std::mutex> UNIQUE_LOCK_NAME(lock_, __LINE__)(_mutex);
#else
#define MUTEX_LOCK(_mutex) \
    std::lock_guard<std::mutex> UNIQUE_LOCK_NAME(lock_, __LINE__)(_mutex);
#endif

// Convert enum to string for logging purposes
std::string_view traceTypeToStr(TraceInstance type);

template <typename Clock, typename Duration>
std::ostream& operator<<(std::ostream& os, const std::chrono::time_point<Clock, Duration>& tp) {
    os << std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count() % 1000000;
    return os;
}

// Base class for traceable objects
class Traceable {
public:
    virtual ~Traceable() = default;

    // Public operator<< that uses the protected trace method
    friend std::ostream& operator<<(std::ostream& os, const Traceable& obj) {
        obj.trace(os);
        return os;
    }

protected:
    // Protected virtual method that derived classes must implement
    virtual void trace(std::ostream& os) const = 0;
};

class FastTraceLogger {

public:
    // Enable or disable global logging
    static void setLoggingEnabled(bool enabled);

    // Enable or disable logging for a specific TraceInstance
    static void setLoggingEnabled(TraceInstance type, bool enabled);

    // Enable or disable logging for a specific ExchangeId
    static void setLoggingEnabled(ExchangeId exchangeId, bool enabled);

    // Check if logging is enabled for a specific ExchangeId
    static bool isLoggingEnabled(ExchangeId exchangeId);

    // Set the log file where logs will be written
    static void setLogFile(const std::string& filename);

    // Convert TraceInstance to a string
    static std::string_view traceTypeToStr(TraceInstance type);

    // Convert ExchangeId to a string
    static std::string_view exchangeIdToStr(ExchangeId exchangeId);

    // Log message with file and line info, plus any additional arguments
    template <typename... Args>
    static void log(std::string level, const Traceable* instance, TraceInstance type, 
        ExchangeId exchangeId, std::string_view file, int line, Args&&... args) {
        std::lock_guard<std::mutex> lock(getMutex());

        // Construct log message
        std::ostringstream oss;
        // timestamp hh:mm::ss.ms
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto time = std::chrono::system_clock::to_time_t(now);

        oss << std::put_time(std::localtime(&time), "%H:%M:%S") << "." << std::setw(3) << std::setfill('0') << ms.count();
        
        oss << " " << level;

        // filename should be 15 characters long
        oss << " " <<std::right << std::setw(15) << std::setfill(' ') << getBaseName(file) << ":"
            << std::left << std::setw(3) << line
            << " [" << traceTypeToStr(type) << "] ";

        // Add exchange ID if available
        if (exchangeId != ExchangeId::UNKNOWN) {
            oss << "[" << exchangeIdToStr(exchangeId) << "] ";
        }
        
        // Print instance if it exists
        if (instance) {
            oss << "[" << *instance << "] ";
        }

        // Print additional args (variadic)
        (oss << ... << std::forward<Args>(args));

        // Get the log stream (file or console)
        std::ostream& out = getOutputStream();

        // Output the log message
        out << oss.str() << std::endl;
    }

    // Specialization for nullptr
    template <typename... Args>
    static void log(std::nullptr_t, TraceInstance type, ExchangeId exchangeId, std::string_view file, int line, Args&&... args) {
        std::lock_guard<std::mutex> lock(getMutex());

        // Construct log message
        std::ostringstream oss;

        // timestamp hh:mm::ss.ms
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
        auto time = std::chrono::system_clock::to_time_t(now);
        oss << std::put_time(std::localtime(&time), "%H:%M:%S") << "." << std::setw(3) << std::setfill('0') << ms.count();

        // filename should be 15 characters long
        oss << " "<< std::right << std::setw(15) << std::setfill(' ') << getBaseName(file) << ":"
            << std::left << std::setw(3) << line
            << " [" << traceTypeToStr(type) << "] ";

        // Add exchange ID if available
        if (exchangeId != ExchangeId::UNKNOWN) {
            oss << "[" << exchangeIdToStr(exchangeId) << "] ";
        }

        oss << std::fixed << std::setprecision(5);

        // Print additional args (variadic)
        (oss << ... << std::forward<Args>(args));

        // Get the log stream (file or console)
        std::ostream& out = getOutputStream();

        // Output the log message
        out << oss.str() << std::endl;
    }

    // countable log takes exact trace type and prints not more than one per 1000 and reset limits every second
    template <typename... Args>
    static void countableLog(std::string level, const Traceable* instance, TraceInstance type, CountableTrace countableTrace,
        ExchangeId exchangeId, std::string_view file, int line, Args&&... args) {
        // increase countable trace
        countableTraces()[static_cast<int>(countableTrace)].fetch_add(1, std::memory_order_relaxed);
        int cnt = countableTraces()[static_cast<int>(countableTrace)].load(std::memory_order_relaxed);
        if (cnt == 1 ||
            cnt % Config::COUNTABLE_TRACES_PRINT_INTERVAL4 == 0 ||
            (cnt < Config::COUNTABLE_TRACES_PRINT_INTERVAL4 && cnt % Config::COUNTABLE_TRACES_PRINT_INTERVAL3 == 0) ||
            (cnt < Config::COUNTABLE_TRACES_PRINT_INTERVAL3 && cnt % Config::COUNTABLE_TRACES_PRINT_INTERVAL2 == 0) ||
            (cnt < Config::COUNTABLE_TRACES_PRINT_INTERVAL2 && cnt % Config::COUNTABLE_TRACES_PRINT_INTERVAL1 == 0)) {
            log(level, instance, type, exchangeId, file, line, "[ cnt:", cnt, "] ", std::forward<Args>(args)...);
        }
    }

    // reset countable traces every second
    static void resetCountableTraces() {
        static std:: string mapIdToString[] = {
            "S_POPLAVKI_OPP",
        };
        const int n = countableTraces().size();
        for (int traceId = 0; traceId < n; traceId++) {
            auto& cnt = countableTraces()[traceId];
            if (cnt > 0) {
                TRACE_BASE(TraceInstance::TRACES, ExchangeId::UNKNOWN, "Resetting countable trace: ", mapIdToString[traceId], " ", cnt.load(std::memory_order_relaxed));
                cnt.store(0, std::memory_order_relaxed);
            }
        }
    }

    // Atomic flag for global logging enable/disable
    static std::atomic<bool>& globalLoggingEnabled();

    // Atomic array for enabling/disabling logging per TraceInstance
    static std::array<std::atomic<bool>, static_cast<int>(TraceInstance::COUNT)>& logLevels();

    // Retrieve the output stream (console or file)
    static std::ostream& getOutputStream();

    // Get the log stream (file)
    static std::ofstream& getLogStream();

    // Mutex for thread safety during logging
    static std::mutex& getMutex();

    // Get the base file name from a full file path
    static constexpr std::string_view getBaseName(std::string_view path) {
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash == std::string_view::npos) {
            return path;  // No path, just the file name
        }
        return path.substr(lastSlash + 1);
    }

private:
    // Add exchange logging levels
    static std::array<std::atomic<bool>, 3>& exchangeLogLevels();  // UNKNOWN, BINANCE, KRAKEN
    
    // array of counters for countable traces
    static std::array<std::atomic<int>, static_cast<int>(CountableTrace::COUNT)>& countableTraces();
};

#endif // TRACER_H
