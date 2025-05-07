#include "api_exchange.h"
#include "api_binance.h"
#include "api_kraken.h"
#include "tracer.h"
#include <stdexcept>
#include <algorithm>

// Define TRACE macro for ApiExchange
#define TRACE(...) TRACE_THIS(TraceInstance::A_EXCHANGE, this->getExchangeId(), __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_EXCHANGE, this->getExchangeId(), __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::A_EXCHANGE, this->getExchangeId(), __VA_ARGS__)

ApiExchange::ApiExchange(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
    const std::string& host, const std::string& port,
    const std::string& restEndpoint, const std::string& wsEndpoint, bool testMode)
    : m_testMode(testMode), m_inCooldown(false), m_cooldownEndTime(std::chrono::steady_clock::now()), 
    m_timersMgr(timersMgr), m_orderBookManager(orderBookManager), m_keepaliveTimerId(0),
    m_host(host), m_port(port), m_restEndpoint(restEndpoint), m_wsEndpoint(wsEndpoint) {
        // Initialize CURL
        m_curl = curl_easy_init();
        if (!m_curl) {
            throw std::runtime_error("Failed to initialize CURL");
        }
    }

ApiExchange::~ApiExchange() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
        m_curl = nullptr;
    }
}

// Factory function to create exchange API instances
std::unique_ptr<ApiExchange> createApiExchange(ExchangeId exchangeId, OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode) {
    if (exchangeId == ExchangeId::BINANCE) {
        return std::make_unique<ApiBinance>(orderBookManager, timersMgr, testMode);
    } else if (exchangeId == ExchangeId::KRAKEN) {
        return std::make_unique<ApiKraken>(orderBookManager, timersMgr, testMode);
    }
    // Add more exchanges here as we implement them
    TRACE_BASE(TraceInstance::A_EXCHANGE, exchangeId, "ERROR: Unsupported exchange");
    return nullptr;
} 

// CURL callback functions
size_t ApiExchange::WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
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


bool ApiExchange::connect() {
    if (m_connected) {
        TRACE("Already connected to ", getExchangeName());
        return true;
    }

    try {
        // Reset IO context if it was stopped
        if (m_ioc.stopped()) {
            m_ioc.restart();
        }

        // Set up SSL context
        m_ctx.set_verify_mode(ssl::verify_peer);
        m_ctx.set_default_verify_paths();

        // Create the WebSocket stream
        m_ws = std::make_unique<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(m_ioc, m_ctx);

        // These two lines are needed for SSL
        if (!SSL_set_tlsext_host_name(m_ws->next_layer().native_handle(), m_host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                                net::error::get_ssl_category()),
                "Failed to set SNI hostname");
        }

        // Look up the domain name
        tcp::resolver resolver(m_ioc);
        auto const results = resolver.resolve(m_host, m_port);

        // Connect to the IP address we get from a lookup
        beast::get_lowest_layer(*m_ws).connect(results);

        // Perform the SSL handshake
        m_ws->next_layer().handshake(ssl::stream_base::client);

        // Perform the websocket handshake
        m_ws->handshake(m_host, m_wsEndpoint);

        m_connected = true;

        // Start the IO context in a separate thread for reading
        m_work = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(m_ioc.get_executor());
        m_thread = std::thread([this]() { 
            TRACE("Starting IO context thread");
            m_ioc.run();
            TRACE("IO context thread finished");
        });

        // Start reading
        doRead();

        TRACE("Successfully connected to ", getExchangeName(), " WebSocket at ", m_host, ":", m_port);
        return true;
    } catch (const std::exception& e) {
        ERROR("Error in connect: ", e.what());
        return false;
    }
}

void ApiExchange::disconnect() {
    if (!m_connected) {
        return;
    }

    try {
        // First, stop the IO context to prevent new operations
        if (m_work) {
            m_work.reset();
        }

        // Stop the IO context
        m_ioc.stop();

        // Wait for the IO thread to finish
        if (m_thread.joinable()) {
            m_thread.join();
        }

        // Reset the IO context for potential reconnection
        m_ioc.restart();

        // Now it's safe to close the WebSocket
        if (m_ws) {
            boost::beast::error_code ec;
            m_ws->close(websocket::close_code::normal, ec);
            if (ec) {
                TRACE("Warning: Error during WebSocket close: ", ec.message());
            }
            m_ws.reset();
        }

        m_connected = false;
        TRACE("Disconnected from ", getExchangeName());
    } catch (const std::exception& e) {
        TRACE("Warning: Error in disconnect: ", e.what());
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

    std::string url = m_restEndpoint + endpoint;
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
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, WriteCallback);
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
        TRACE("Response: ", response.substr(0, 500));
        return json::parse(response);
    } catch (const json::parse_error& e) {
        TRACE("Failed to parse JSON response: ", e.what());
        TRACE("Response: ", response);
        throw std::runtime_error("Failed to parse JSON response");
    }
}

void ApiExchange::doRead() {
    m_ws->async_read(m_buffer,
        [this](boost::beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                TRACE("Read error: ", ec.message());
                return;
            }

            std::string message = boost::beast::buffers_to_string(m_buffer.data());
            m_buffer.consume(m_buffer.size());
            
            // Truncate long messages for logging
            std::string logMessage = message;
            if (logMessage.length() > 500) {
                logMessage = logMessage.substr(0, 497) + "...";
            }
            DEBUG("Received message: ", logMessage);
            
            processMessage(message);
            doRead();
        });
}

void ApiExchange::doWrite(std::string message) {
    TRACE("Sending WebSocket message: ", message);
    m_ws->async_write(net::buffer(message),
        [this, message](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                TRACE("Write error: ", ec.message(), " for message: ", message);
                return;
            }
        });
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

void ApiExchange::setSnapshotCallback(std::function<void(bool)> callback) {
    m_snapshotCallback = callback;
}

void ApiExchange::setOrderCallback(std::function<void(bool)> callback) {
    m_orderCallback = callback;
}

void ApiExchange::setBalanceCallback(std::function<void(bool)> callback) {
    m_balanceCallback = callback;
}