#include <iostream>
#include <fstream>
#include <atomic>
#include <mutex>
#include <ctime>
#include <sstream>
#include <iomanip>
#include "tracer.h"

std::atomic<bool> g_globalLoggingEnabled(true);
std::array<std::atomic<bool>, static_cast<int>(TraceInstance::COUNT)> g_logLevels{};
std::ofstream g_logFile;
std::mutex g_logMutex;

void FastTraceLogger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(g_logMutex);
    if (g_logFile.is_open()) {
        g_logFile.close();
    }
    std::cout << "Logs will be written to: " << filename << std::endl;
    g_logFile.open(filename, std::ios::app);  // Open in append mode
}

void FastTraceLogger::setLoggingEnabled(bool enabled) {
    g_globalLoggingEnabled.store(enabled, std::memory_order_relaxed);

    // enable for all types
    for (auto& level : g_logLevels) {
        level.store(enabled, std::memory_order_relaxed);
    }
}

void FastTraceLogger::setLoggingEnabled(TraceInstance type, bool enabled) {
    g_logLevels[static_cast<int>(type)].store(enabled, std::memory_order_relaxed);
}

std::ostream& FastTraceLogger::getOutputStream() {
    if (g_logFile.is_open()) {
        return g_logFile;
    }
    return std::cout;
}

std::ofstream& FastTraceLogger::getLogStream() {
    return g_logFile;
}

std::mutex& FastTraceLogger::getMutex() {
    return g_logMutex;
}

std::atomic<bool>& FastTraceLogger::globalLoggingEnabled() {
    return g_globalLoggingEnabled;
}

std::array<std::atomic<bool>, static_cast<int>(TraceInstance::COUNT)>& FastTraceLogger::logLevels() {
    return g_logLevels;
}

std::string_view FastTraceLogger::traceTypeToStr(TraceInstance type) {
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

/* Example

// ✅ Example class with overloaded `operator<<`
class MyClass {
public:
    friend std::ostream& operator<<(std::ostream& os, const MyClass* obj) {
        return os << "MyClass@" << static_cast<const void*>(obj);
    }

    void testTrace(int id) {
        TRACE_THIS(TraceInstance::TIMER, "called with ", id, " and more data");
    }
};

int main() {
    MyClass obj;

    // ✅ Log to a file
    FastTraceLogger::setLogFile("app.log");

    // ✅ Enable/disable logging at runtime
    FastTraceLogger::setLoggingEnabled(true); // Enable all logs
    FastTraceLogger::setLoggingEnabled(TraceInstance::TIMER, false); // Disable TIMER logs

    obj.testTrace(42); // This won't log because TIMER is off

    FastTraceLogger::setLoggingEnabled(TraceInstance::TIMER, true); // Enable TIMER logs
    obj.testTrace(100); // This will log

    return 0;
}

*/
