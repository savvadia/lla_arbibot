#pragma once
#ifndef TRACER_H
#define TRACER_H

#include <iostream>
#include <fstream>
#include <mutex>
#include <atomic>
#include <string_view>
#include <unordered_map>
#include <array>
#include <string>
#include <sstream>
#include <iomanip>

// Enum for logging types
enum class TraceInstance {
    TIMER,
    BALANCE,
    EVENT_LOOP,
    EX_MGR,
    STRAT,
    ORDERBOOK,
    EVENTLOOP,
    A_EXCHANGE,
    A_KRAKEN,
    A_BINANCE,
    MAIN,  // For main function logging
    COUNT,  // To track the number of log types
    MUTEX
};

// Convert enum to string for logging purposes
std::string_view traceTypeToStr(TraceInstance type);

template <typename Clock, typename Duration>
std::ostream& operator<<(std::ostream& os, const std::chrono::time_point<Clock, Duration>& tp) {
    os << std::chrono::duration_cast<std::chrono::microseconds>(tp.time_since_epoch()).count();
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

    // Set the log file where logs will be written
    static void setLogFile(const std::string& filename);

    // Convert TraceInstance to a string
    static std::string_view traceTypeToStr(TraceInstance type);

    // Log message with file and line info, plus any additional arguments
    template <typename... Args>
    static void log(std::string level, const Traceable* instance, TraceInstance type, std::string_view file, int line, Args&&... args) {
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

        oss << std::fixed << std::setprecision(3);
        
        // Print instance if it exists
        if (instance) {
            oss << " [" << *instance << "]";
        }

        oss << " ";

        // Print additional args (variadic)
        (oss << ... << std::forward<Args>(args));

        // Get the log stream (file or console)
        std::ostream& out = getOutputStream();

        // Output the log message
        out << oss.str() << std::endl;
    }

    // Specialization for nullptr
    template <typename... Args>
    static void log(std::nullptr_t, TraceInstance type, std::string_view file, int line, Args&&... args) {
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

        // Print additional args (variadic)
        (oss << ... << std::forward<Args>(args));

        // Get the log stream (file or console)
        std::ostream& out = getOutputStream();

        // Output the log message
        out << oss.str() << std::endl;
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

    // Utility to get the base file name from a full file path
    static constexpr std::string_view getBaseName(std::string_view path) {
        size_t lastSlash = path.find_last_of("/\\");
        if (lastSlash == std::string_view::npos) {
            return path;  // No path, just the file name
        }
        return path.substr(lastSlash + 1);
    }
};

// Macro for TRACE with conditional logging (enabled only if logging is enabled)
#ifndef DISABLED_TRACE
    #define TRACE_OBJ(_level, _obj, _type, ...)                                            \
        if (FastTraceLogger::globalLoggingEnabled().load(std::memory_order_relaxed) &&  \
            FastTraceLogger::logLevels()[static_cast<int>(_type)].load(std::memory_order_relaxed)) { \
            FastTraceLogger::log(_level, _obj, _type, __FILE__, __LINE__, __VA_ARGS__); \
        }
#else
    #define TRACE_OBJ(_obj, _type, ...) ((void)0)  // Compiler removes the call entirely
#endif
#define TRACE_THIS(_type, ...) TRACE_OBJ("INFO ", this, _type, __VA_ARGS__)
#define TRACE_BASE(_type, ...) TRACE_OBJ("INFO ",nullptr, _type, __VA_ARGS__)

#if 0
    #define DEBUG_THIS(_type, ...) TRACE_OBJ("DEBUG", this, _type, __VA_ARGS__)
    #define DEBUG_BASE(_type, ...) TRACE_OBJ("DEBUG",nullptr, _type, __VA_ARGS__)
#else
    #define DEBUG_THIS(_type, ...) ((void)0)
    #define DEBUG_BASE(_type, ...) ((void)0)
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

#endif // TRACER_H
