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
    BOOK,
    EX_MGR,
    STRAT,
    ORDERBOOK,
    EVENTLOOP,
    API,
    MAIN,  // For main function logging
    COUNT  // To track the number of log types
};

// Convert enum to string for logging purposes
constexpr std::string_view traceTypeToStr(TraceInstance type) {
    switch (type) {
        case TraceInstance::TIMER:      return "TIMER";
        case TraceInstance::BALANCE:    return "BALANCE";
        case TraceInstance::EVENT_LOOP: return "EVENT_LOOP";
        case TraceInstance::BOOK:       return "BOOK";
        case TraceInstance::EX_MGR:     return "EX_MGR";
        case TraceInstance::STRAT:      return "STRAT";
        case TraceInstance::ORDERBOOK:  return "ORDERBOOK";
        case TraceInstance::EVENTLOOP:  return "EVENTLOOP";
        case TraceInstance::API:        return "API";
        case TraceInstance::MAIN:       return "MAIN";
        default: return "UNKNOWN";
    }
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

    // Log message with file and line info, plus any additional arguments
    template <typename... Args>
    static void log(const Traceable* instance, TraceInstance type, std::string_view file, int line, Args&&... args) {
        std::lock_guard<std::mutex> lock(getMutex());

        // Construct log message
        std::ostringstream oss;
        // filename should be 15 characters long
        oss << std::right << std::setw(15) << getBaseName(file) << ":"
            << std::left << std::setw(3) << line
            << " [" << traceTypeToStr(type) << "] ";

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
        // filename should be 15 characters long
        oss << std::right << std::setw(15) << getBaseName(file) << ":"
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

    // Convert TraceInstance to a string
    static std::string traceTypeToStr(TraceInstance type);
};

// Macro for TRACE with conditional logging (enabled only if logging is enabled)
#ifndef DISABLED_TRACE
    #define TRACE_OBJ(_obj, _type, ...)                                            \
        if (FastTraceLogger::globalLoggingEnabled().load(std::memory_order_relaxed) &&  \
            FastTraceLogger::logLevels()[static_cast<int>(_type)].load(std::memory_order_relaxed)) { \
            FastTraceLogger::log(_obj, _type, __FILE__, __LINE__, __VA_ARGS__); \
        }
#else
    #define TRACE_OBJ(_obj, _type, ...) ((void)0)  // Compiler removes the call entirely
#endif
    #define TRACE_THIS(_type, ...) TRACE_OBJ(this, _type, __VA_ARGS__)
    #define TRACE_BASE(_type, ...) TRACE_OBJ(nullptr, _type, __VA_ARGS__)

#endif // TRACER_H
