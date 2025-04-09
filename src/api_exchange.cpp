#include "api_exchange.h"
#include "api_binance.h"
#include "api_kraken.h"
#include <stdexcept>

// Define TRACE macro for ApiExchange
#define TRACE(...) TRACE_THIS(TraceInstance::A_EXCHANGE, __VA_ARGS__)

std::unique_ptr<ApiExchange> createApiExchange(const std::string& exchangeName, OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode) {
    if (exchangeName == "Binance") {
        return std::make_unique<ApiBinance>(orderBookManager, timersMgr, testMode);
    } else if (exchangeName == "Kraken") {
        return std::make_unique<ApiKraken>(orderBookManager, timersMgr, testMode);
    }
    // Add more exchanges here as we implement them
    throw std::runtime_error("Unsupported exchange: " + exchangeName);
} 

void ApiExchange::startCooldown(int minutes) {
    std::lock_guard<std::mutex> lock(m_cooldownMutex);
    m_inCooldown = true;
    m_cooldownEndTime = std::chrono::steady_clock::now() + std::chrono::minutes(minutes);
    TRACE(getExchangeName(), " entering cooldown for ", minutes, " minutes");
}

void ApiExchange::endCooldown() {
    std::lock_guard<std::mutex> lock(m_cooldownMutex);
    m_inCooldown = false;
    TRACE(getExchangeName(), " cooldown ended");
}

bool ApiExchange::checkCooldownExpired() {
    std::lock_guard<std::mutex> lock(m_cooldownMutex);
    if (!m_inCooldown) return true;
    
    auto now = std::chrono::steady_clock::now();
    if (now >= m_cooldownEndTime) {
        m_inCooldown = false;
        TRACE(getExchangeName(), " cooldown expired");
        return true;
    }
    
    return false;
}

void ApiExchange::updateRateLimit(const std::string& endpoint, int limit, int remaining, int reset) {
    // Store rate limit info for this endpoint
    m_rateLimits[endpoint + "_limit"] = limit;
    m_rateLimits[endpoint + "_remaining"] = remaining;
    m_rateLimits[endpoint + "_reset"] = reset;
    
    // If we're close to the limit, consider entering a cooldown
    if (remaining < limit * 0.1) { // Less than 10% remaining
        int cooldownMinutes = std::max(1, reset / 60);
        startCooldown(cooldownMinutes);
        TRACE(getExchangeId(), " approaching rate limit for ", endpoint, 
                " (", remaining, "/", limit, " remaining, reset in ", reset, "s)");
    }
}

void ApiExchange::handleHttpError(int httpCode, const std::string& response, const std::string& endpoint) {
    std::string errorMsg;

    switch (httpCode) {
        case 403:
            errorMsg = "Forbidden - API key may be invalid or have insufficient permissions";
            break;
        case 408:
            errorMsg = "Request timeout";
            break;
        case 429:
            errorMsg = "Rate limit exceeded";
            break;
        case 418:
            errorMsg = "IP has been auto-banned for continuing to send requests after receiving 429 codes";
            break;
        case 503:
            if (response.find("Unknown error") != std::string::npos) {
                errorMsg = "Request sent but no response within timeout period";
            } else if (response.find("Service Unavailable") != std::string::npos) {
                errorMsg = "Service unavailable at the moment";
            } else if (response.find("Internal error") != std::string::npos) {
                errorMsg = "Internal error; unable to process request";
            }
            break;
        default:
            if (httpCode >= 400 && httpCode < 500) {
                errorMsg = "Client error: " + std::to_string(httpCode);
            } else if (httpCode >= 500) {
                errorMsg = "Server error: " + std::to_string(httpCode);
            }
    }

    if (!errorMsg.empty()) {
        TRACE("HTTP Error ", httpCode, " for ", getExchangeName(), " endpoint ", endpoint, ": ", errorMsg);
        
        // Try to parse error details from response
        try {
            if (response.find("code") != std::string::npos || response.find("error") != std::string::npos) {
                // Log the raw response for debugging
                TRACE("Error response: ", response);
            }
        } catch (...) {
            TRACE("Raw error response: ", response);
        }
    }

    // Call the exchange-specific cooldown method
    cooldown(httpCode, response, endpoint);
}