#pragma once

#include <string>
#include <chrono>
#include <iomanip>
#include <sstream>

inline std::string getTestTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_c), "%H:%M:%S.") 
       << std::setfill('0') << std::setw(3) 
       << std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 1000;
    return ss.str();
} 