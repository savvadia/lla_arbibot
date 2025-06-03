#include "api_exchange.h"
#include "api_binance.h"
#include "api_kraken.h"
#include "api_kucoin.h"
#include "api_bybit.h"
#include <stdexcept>
#include <algorithm>
#include "tracer.h"
#include "config.h"

// Define TRACE macro for ApiExchange
#define TRACE(...) TRACE_THIS(TraceInstance::A_EXCHANGE, this->getExchangeId(), __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_EXCHANGE, this->getExchangeId(), __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::A_EXCHANGE, this->getExchangeId(), __VA_ARGS__)
#define ERROR_CNT(_id, ...) ERROR_COUNT(TraceInstance::A_EXCHANGE, _id, this->getExchangeId(), __VA_ARGS__)

#define TRACE_IO(...) TRACE_THIS(TraceInstance::A_IO, this->getExchangeId(), __VA_ARGS__)
#define DEBUG_IO(...) DEBUG_THIS(TraceInstance::A_IO, this->getExchangeId(), __VA_ARGS__)
// Timer callback for snapshot validity check
void snapshotValidityCheckCallback(int id, void* data) {
    auto* exchange = static_cast<ApiExchange*>(data);
    TRACE_BASE(TraceInstance::A_EXCHANGE, exchange->getExchangeId(), "Checking snapshot validity");
    if(exchange->checkSnapshotValidity() == ApiExchange::SnapshotRestoring::IN_PROGRESS) {
        exchange->startSnapshotValidityTimer(Config::SNAPSHOT_VALIDITY_CHECK_INTERVAL_PROLONGED_MS);
    } else {
        exchange->startSnapshotValidityTimer();
    }
}

ApiExchange::ApiExchange(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
    const std::string& restEndpoint, 
    const std::string& wsHost, const std::string& wsPort, const std::string& wsEndpoint,
    const std::vector<TradingPair> pairs, bool testMode)
    : m_testMode(testMode), m_inCooldown(false), m_cooldownEndTime(std::chrono::steady_clock::now()), 
    m_timersMgr(timersMgr), m_orderBookManager(orderBookManager), 
        m_restEndpoint(restEndpoint), 
        m_wsHost(wsHost), m_wsPort(wsPort), m_wsEndpoint(wsEndpoint),
    m_pairs(pairs) {
    
    // Initialize symbol states
    for (const auto& pair : pairs) {
        symbolStates[pair] = SymbolState{};
    }

    // Initialize CURL
    initCurl();
}

ApiExchange::~ApiExchange() {
    disconnect();
    cleanupCurl();
    m_timersMgr.stopTimer(m_snapshotValidityTimerId);
}

// Factory function to create exchange API instances
std::unique_ptr<ApiExchange> createApiExchange(ExchangeId exchangeId, OrderBookManager& orderBookManager, TimersMgr& timersMgr,
    const std::vector<TradingPair> pairs, bool testMode) {
    if (exchangeId == ExchangeId::BINANCE) {
        return std::make_unique<ApiBinance>(orderBookManager, timersMgr, pairs, testMode);
    } else if (exchangeId == ExchangeId::KRAKEN) {
        return std::make_unique<ApiKraken>(orderBookManager, timersMgr, pairs, testMode);
    } else if (exchangeId == ExchangeId::KUCOIN) {
        return std::make_unique<ApiKucoin>(orderBookManager, timersMgr, pairs, testMode);
    } else if (exchangeId == ExchangeId::BYBIT) {
        return std::make_unique<ApiBybit>(orderBookManager, timersMgr, pairs, testMode);
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

size_t ApiExchange::WriteHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t totalSize = size * nitems;
    std::string* headers = static_cast<std::string*>(userdata);
    headers->append(buffer, totalSize);
    return totalSize;
}

std::string ApiExchange::formatCurlHeaders(struct curl_slist* headers) {
    std::string result;
    for (struct curl_slist* temp = headers; temp != nullptr; temp = temp->next) {
        result += temp->data;
        result += "; ";
    }
    if (!result.empty()) {
        result.pop_back(); // remove last space
        result.pop_back(); // remove last semicolon
    }
    return result;
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
        if (!SSL_set_tlsext_host_name(m_ws->next_layer().native_handle(), m_wsHost.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                                net::error::get_ssl_category()),
                "Failed to set SNI hostname");
        }

        // Look up the domain name
        tcp::resolver resolver(m_ioc);
        auto const results = resolver.resolve(m_wsHost, m_wsPort);

        // Connect to the IP address we get from a lookup
        beast::get_lowest_layer(*m_ws).connect(results);

        // Perform the SSL handshake
        m_ws->next_layer().handshake(ssl::stream_base::client);

        // Perform the websocket handshake
        std::string target = m_wsEndpoint;
        if (target.empty() || target[0] != '/') {
            target = "/" + target;
        }
        m_ws->handshake(m_wsHost, target);

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

        TRACE("Successfully connected to ", getExchangeName(), " WebSocket at ", m_wsHost, ":", m_wsPort);
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

        // update state now in case of faults below
        m_connected = false;

        // Close the WebSocket connection first
        if (m_ws) {
            boost::beast::error_code ec;
            m_ws->close(websocket::close_code::normal, ec);
            if (ec) {
                TRACE("Warning: Error during WebSocket close: ", ec.message());
            }
            m_ws.reset();
        }

        // Stop the IO context
        m_ioc.stop();

        // Wait for the IO thread to finish with a timeout
        if (m_thread.joinable()) {
            auto timeout = std::chrono::steady_clock::now() + std::chrono::seconds(5);
            while (m_thread.joinable() && std::chrono::steady_clock::now() < timeout) {
                try {
                    m_thread.join();
                    break;
                } catch (const std::exception& e) {
                    TRACE("Warning: Error joining thread: ", e.what());
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
            if (m_thread.joinable()) {
                TRACE("Warning: Could not join thread within timeout");
                m_thread.detach();
            }
        }

        // Reset the IO context for potential reconnection
        m_ioc.restart();

        TRACE("Disconnected from ", getExchangeName());
    } catch (const std::exception& e) {
        TRACE("Warning: Error in disconnect: ", e.what());
    }
}

// Common HTTP request handling
json ApiExchange::makeHttpRequest(const std::string& endpoint, const std::string& params, const std::string& method, bool addJsonHeader) {
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

    std::string response;
    std::string headerData;
    long httpCode = 0;

    // Initialize headers list
    struct curl_slist* requestHeaders = nullptr;
    if (addJsonHeader) {
        requestHeaders = curl_slist_append(requestHeaders, "Content-Type: application/json");

        if (!requestHeaders) {
            throw std::runtime_error("Failed to create headers list");
        }
    }

    TRACE_IO("Making HTTP ", method, " request to: ", url, " with params cnt: ", params.size(), " and headers: ", formatCurlHeaders(requestHeaders));

    // Reset first if needed
    curl_easy_reset(m_curl);
    curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);


    // Now set all options
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(m_curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(m_curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(m_curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(m_curl, CURLOPT_HEADERFUNCTION, WriteHeaderCallback);
    curl_easy_setopt(m_curl, CURLOPT_HEADERDATA, &headerData);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(m_curl, CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, requestHeaders);

    if (method == "DELETE") {
        curl_easy_setopt(m_curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "POST") {
        curl_easy_setopt(m_curl, CURLOPT_POST, 1L);
        if (params.empty()) {
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, "");
        } else {
            curl_easy_setopt(m_curl, CURLOPT_POSTFIELDS, params.c_str());
        }
        curl_easy_setopt(m_curl, CURLOPT_POSTFIELDSIZE, params.size());
    } else {
        curl_easy_setopt(m_curl, CURLOPT_HTTPGET, 1L);
    }

    curl_easy_setopt(m_curl, CURLOPT_DNS_CACHE_TIMEOUT, 60L);
    // curl_easy_setopt(m_curl, CURLOPT_VERBOSE, 1L);
    
    DEBUG("Starting CURL request...");
    
    // Perform the request with error checking
    CURLcode res = curl_easy_perform(m_curl);

    if (res != CURLE_OK) {
        ERROR("CURL request failed with code ", res, ": ", curl_easy_strerror(res));
        
        // Clean up headers list
        if (requestHeaders) {
            curl_slist_free_all(requestHeaders);
        }
        
        throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }
    
    // Get response code
    curl_easy_getinfo(m_curl, CURLINFO_RESPONSE_CODE, &httpCode);
    DEBUG_IO("HTTP response code: ", httpCode);
    
    // Clean up headers list
    if (requestHeaders) {
        curl_slist_free_all(requestHeaders);
    }
    
    // Process rate limit headers
    if (!headerData.empty()) {
        processRateLimitHeaders(headerData);
    }
    
    // Handle HTTP errors
    if (httpCode >= 400) {
        ERROR("HTTP error ", httpCode, " for endpoint ", endpoint);
        handleHttpError(httpCode, response, endpoint);
    }
    
    // Parse response as JSON
    try {
        DEBUG_IO("Response: ", response.substr(0, 500));
        return json::parse(response);
    } catch (const json::parse_error& e) {
        ERROR("Failed to parse JSON response: ", e.what(), " for response: ", response);
        throw std::runtime_error("Failed to parse JSON response");
    }
}

void ApiExchange::doRead() {
    std::lock_guard<std::mutex> lock(m_wsMutex);
    if (!m_ws) return;

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
    std::lock_guard<std::mutex> lock(m_wsMutex);
    if (!m_ws) return;

    TRACE_BASE(TraceInstance::A_IO, getExchangeId(), "Sending: ", message);
    
    // Add message to queue
    m_writeQueue.push(std::move(message));
    
    // If not already writing, start the write chain
    if (!m_isWriting) {
        writeNext();
    }
}

void ApiExchange::writeNext() {
    if (!m_ws || m_writeQueue.empty()) {
        m_isWriting = false;
        return;
    }

    m_isWriting = true;
    std::string message = std::move(m_writeQueue.front());
    m_writeQueue.pop();

    m_ws->async_write(net::buffer(message),
        [this, message](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                ERROR_CNT(CountableTrace::A_EXCHANGE_WRITE_ERROR, ec.message(), " for message: ", message);
                m_isWriting = false;
                return;
            }

            // Write next message if any
            std::lock_guard<std::mutex> lock(m_wsMutex);
            writeNext();
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

void ApiExchange::startSnapshotValidityTimer(int intervalMs) {
    m_snapshotValidityTimerId = m_timersMgr.addTimer(
        intervalMs,
        snapshotValidityCheckCallback,
        this,
        TimerType::EXCHANGE_CHECK_SNAPSHOT_VALIDITY
    );
}

ApiExchange::SnapshotRestoring ApiExchange::checkSnapshotValidity() {
    if (!m_connected) {
        ERROR_CNT(CountableTrace::A_EXCHANGE_NOT_CONNECTED,
            getExchangeName(), ": Not connected to ", getExchangeName(), ". Skipping snapshot validity check");
        return ApiExchange::SnapshotRestoring::NONE;
    }

    auto now = std::chrono::system_clock::now();
    std::vector<TradingPair> needResubscribe;
    std::ostringstream needResubscribeStr;
    for (const auto& pair : m_pairs) {
        auto& state = symbolStates[pair];
        if (!state.hasSnapshot()) {
            ERROR_CNT(CountableTrace::A_EXCHANGE_SNAPSHOT_MISSING, pair, ": Snapshot missing");
            needResubscribe.push_back(pair);
            continue;
        }

        // Get the last update time from the order book
        auto lastUpdate = m_orderBookManager.getOrderBook(getExchangeId(), pair).getLastUpdate();
        auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate).count();

        if (timeSinceLastUpdate > Config::SNAPSHOT_VALIDITY_TIMEOUT_MS) {
            ERROR_CNT(CountableTrace::A_EXCHANGE_SNAPSHOT_STALE,
                pair, ": Snapshot for ", pair, " is stale (", timeSinceLastUpdate, "ms old). Resubscribing...");
            setSymbolSnapshotState(pair, false);
            needResubscribe.push_back(pair);
            needResubscribeStr << pair << ", ";
        } else {
            DEBUG("Snapshot for ", pair, " is valid (", timeSinceLastUpdate, "ms old)");
        }
    }

    if (!needResubscribe.empty()) {
        resubscribeOrderBook(needResubscribe);
        TRACE("re-subscribed: ", needResubscribeStr.str());
        return ApiExchange::SnapshotRestoring::IN_PROGRESS;
    }

    return ApiExchange::SnapshotRestoring::NONE;
}

void ApiExchange::processMessage(std::string message) {
    TRACE("Processing message: ", message.substr(0, 500));
    try {
        json parsedMessage = json::parse(message);
        processMessage(parsedMessage);
    } catch (const json::parse_error& e) {
        ERROR("Error parsing message: ", e.what(), " message: ", message);
    } catch (const std::exception& e) {
        ERROR("Error processing message: ", e.what(), " message: ", message);
    }
}