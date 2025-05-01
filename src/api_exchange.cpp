#include "api_exchange.h"
#include "api_binance.h"
#include "api_kraken.h"
#include <stdexcept>
#include <algorithm>

// Define TRACE macro for ApiExchange
#define TRACE(...) TRACE_THIS(TraceInstance::A_EXCHANGE, this->getExchangeId(), __VA_ARGS__)

std::unique_ptr<ApiExchange> createApiExchange(const std::string& exchangeName, OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode) {
    if (exchangeName == "Binance") {
        return std::make_unique<ApiBinance>(orderBookManager, timersMgr, testMode);
    } else if (exchangeName == "Kraken") {
        return std::make_unique<ApiKraken>(orderBookManager, timersMgr, testMode);
    }
    // Add more exchanges here as we implement them
    throw std::runtime_error("Unsupported exchange: " + exchangeName);
} 

// CURL callback functions
size_t ApiExchange::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

size_t ApiExchange::HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    ((std::string*)userdata)->append(buffer, size * nitems);
    return size * nitems;
}

// Initialize CURL
bool ApiExchange::initCurl() {
    if (m_curl) {
        return true; // Already initialized
    }
    
    m_curl = curl_easy_init();
    if (!m_curl) {
        TRACE("Failed to initialize CURL");
        return false;
    }
    
    return true;
}

// Clean up CURL
void ApiExchange::cleanupCurl() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
        m_curl = nullptr;
    }
}

// Common HTTP request handling
json ApiExchange::makeHttpRequest(const std::string& endpoint, const std::string& params, const std::string& method) {
    // Check if we're in a cooldown period
    if (isInCooldown()) {
        int remainingSeconds = getRemainingCooldownSeconds();
        TRACE(getExchangeName(), " API in cooldown for ", remainingSeconds, " more seconds. Skipping request to ", endpoint);
        throw std::runtime_error("API in cooldown period");
    }

    if (!m_curl) {
        if (!initCurl()) {
            throw std::runtime_error("CURL not initialized");
        }
    }

    std::string url = getRestEndpoint() + endpoint;
    if (!params.empty() && method == "GET") {
        url += "?" + params;
    }

    TRACE("Making HTTP ", method, " request to: ", url);

    std::string response;
    std::string headers;
    long httpCode = 0;
    
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &headers);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    if (method == "DELETE") {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "POST") {
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        if (!params.empty()) {
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, params.c_str());
        }
    } else {
        curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
    }
    
    CURLcode res = curl_easy_perform(m_curl);
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }

    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    // Process rate limit headers
    if (!headers.empty()) {
        processRateLimitHeaders(headers);
    }
    
    // Handle HTTP errors
    if (httpCode >= 400) {
        handleHttpError(httpCode, response, endpoint);
    }
    
    // Parse response as JSON
    try {
        return json::parse(response);
    } catch (const json::parse_error& e) {
        TRACE("Failed to parse JSON response: ", e.what());
        TRACE("Response: ", response);
        throw std::runtime_error("Failed to parse JSON response");
    }
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
    
    TRACE(getExchangeName(), " rate limit for ", endpoint, ": ", remaining, "/", limit, " (reset in ", reset, "s)");
    
    // If we're close to the limit, start a cooldown
    if (remaining < limit * 0.1) { // Less than 10% remaining
        int cooldownMinutes = std::max(1, reset / 60);
        startCooldown(cooldownMinutes);
    }
}

void ApiExchange::cooldown(int httpCode, const std::string& response, const std::string& endpoint) {
    // Default implementation - can be overridden by specific exchanges
    bool shouldCooldown = false;
    int cooldownMinutes = 5; // Default cooldown time

    switch (httpCode) {
        case 403:
            shouldCooldown = true;
            cooldownMinutes = 60; // Longer cooldown for auth issues
            break;
        case 408:
            shouldCooldown = true;
            cooldownMinutes = 5;
            break;
        case 429:
            shouldCooldown = true;
            cooldownMinutes = 30; // Longer cooldown for rate limits
            break;
        case 418:
            shouldCooldown = true;
            cooldownMinutes = 120; // Much longer cooldown for bans
            break;
        case 503:
            shouldCooldown = true;
            cooldownMinutes = 15;
            break;
        default:
            if (httpCode >= 400 && httpCode < 500) {
                shouldCooldown = true;
                cooldownMinutes = 10;
            } else if (httpCode >= 500) {
                shouldCooldown = true;
                cooldownMinutes = 15;
            }
    }

    if (shouldCooldown) {
        startCooldown(cooldownMinutes);
    }
}

void ApiExchange::handleHttpError(int httpCode, const std::string& response, const std::string& endpoint) {
    TRACE(getExchangeName(), " HTTP error ", httpCode, " for endpoint ", endpoint);
    
    // Try to parse the error response
    try {
        json errorJson = json::parse(response);
        TRACE(getExchangeName(), " Error response: ", errorJson.dump());
    } catch (const json::parse_error&) {
        TRACE(getExchangeName(), " Raw error response: ", response);
    }
    
    // Apply cooldown based on the error code
    cooldown(httpCode, response, endpoint);
    
    // Throw an exception with the error details
    std::string errorMsg = getExchangeName() + " API error " + std::to_string(httpCode);
    if (!endpoint.empty()) {
        errorMsg += " for endpoint " + endpoint;
    }
    throw std::runtime_error(errorMsg);
}