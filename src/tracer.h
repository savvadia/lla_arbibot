#include <cstdio>

enum TraceInstance {
    TRACE_TIMER,
    TRACE_BALANCE,
};

// Compile-time function to extract only the filename without path
constexpr const char* extractFilename(const char* path) {
    const char* filename = path;
    while (*path) {
        if (*path == '/' || *path == '\\') {
            filename = path + 1;
        }
        ++path;
    }
    return filename;
}

#define TRACE_INST_TIMER(_timer, ...) \
    do { \
        constexpr const char* filename = extractFilename(__FILE__); \
        printf("%s:%d [TIMER] [%d %s] ", filename, __LINE__, _timer.id, _timer.formatFireTime().c_str()); \
        printf(__VA_ARGS__); \
        printf("\n"); \
    } while (0)

// adds \n at the end
#define TRACE_INST(instance, ...) \
    do { \
        const unordered_map<TraceInstance, string> traceNames = { \
            {TRACE_TIMER, "TIMER"}, \
            {TRACE_BALANCE, "BALANCE"}, \
        }; \
        constexpr const char* filename = extractFilename(__FILE__); \
        printf("%s:%d [%s] ", filename, __LINE__, traceNames.at(instance).c_str()); \
        printf(__VA_ARGS__); \
        printf("\n"); \
    } while (0)
