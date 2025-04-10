#pragma once

#include "types.h"
#include "orderbook.h"
#include "timers.h"
#include <string>
#include <functional>
#include <memory>
#include "tracer.h"
#include <chrono>
#include <mutex>
#include <atomic>
#include <map>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

using json = nlohmann::json;

// Base class for exchange APIs
class ApiExchange : public Traceable {
public:
    ApiExchange(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode = true) : 
        m_testMode(testMode), m_inCooldown(false), m_cooldownEndTime(std::chrono::steady_clock::now()), m_timersMgr(timersMgr), m_orderBookManager(orderBookManager), m_keepaliveTimerId(0), m_curl(nullptr) {}
    virtual ~ApiExchange() {
        if (m_curl) {
            curl_easy_cleanup(m_curl);
            m_curl = nullptr;
        }
    }

    // Connect to the exchange
    virtual bool connect() = 0;

    // Disconnect from the exchange
    virtual void disconnect() = 0;

    // Subscribe to order book updates for a trading pair
    virtual bool subscribeOrderBook(std::vector<TradingPair> pairs) = 0;

    // Get order book snapshot for a trading pair
    virtual bool getOrderBookSnapshot(TradingPair pair) = 0;

    // Process incoming messages
    virtual void processMessages() = 0;

    // Order management
    virtual bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) = 0;
    virtual bool cancelOrder(const std::string& orderId) = 0;
    virtual bool getBalance(const std::string& asset) = 0;

    // Get exchange name
    virtual std::string getExchangeName() const = 0;

    // Get exchange ID
    virtual ExchangeId getExchangeId() const = 0;

    // Check if connected to the exchange
    virtual bool isConnected() const = 0;

    // Check if in test mode
    virtual bool isTestMode() const { return m_testMode; }

    // Check if in cooldown period
    virtual bool isInCooldown() const { return m_inCooldown; }

    // Get remaining cooldown time in seconds
    virtual int getRemainingCooldownSeconds() const {
        if (!m_inCooldown) return 0;
        
        auto now = std::chrono::steady_clock::now();
        auto remaining = std::chrono::duration_cast<std::chrono::seconds>(m_cooldownEndTime - now).count();
        return remaining > 0 ? remaining : 0;
    }

    // Callbacks
    virtual void setSubscriptionCallback(std::function<void(bool)> callback) = 0;
    virtual void setSnapshotCallback(std::function<void(bool)> callback) = 0;
    virtual void setOrderCallback(std::function<void(bool)> callback) = 0;
    virtual void setBalanceCallback(std::function<void(bool)> callback) = 0;

    // Tracing
    void trace(std::ostream& os) const override {
        os << getExchangeName();
    }

    // Utility functions
    static std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    // Common HTTP request handling
    virtual json makeHttpRequest(const std::string& endpoint, const std::string& params = "", const std::string& method = "GET");
    
    // Initialize CURL
    virtual bool initCurl();
    
    // Clean up CURL
    virtual void cleanupCurl();

    // Cooldown and rate limiting
    virtual void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "");
    virtual void handleHttpError(int httpCode, const std::string& response, const std::string& endpoint = "");
    virtual void startCooldown(int minutes);
    virtual void endCooldown();
    virtual bool checkCooldownExpired();
    virtual void updateRateLimit(const std::string& endpoint, int limit, int remaining, int reset);

protected:
    bool m_testMode;
    bool m_inCooldown;
    std::chrono::steady_clock::time_point m_cooldownEndTime;
    std::mutex m_cooldownMutex;
    std::map<std::string, int> m_rateLimits;
    TimersMgr& m_timersMgr;
    OrderBookManager& m_orderBookManager;
    uint64_t m_keepaliveTimerId;
    CURL* m_curl;
    
    // CURL callback functions
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    
    // Helper methods for derived classes
    virtual void processRateLimitHeaders(const std::string& headers) = 0;
    virtual std::string getRestEndpoint() const = 0;
};

// Factory function to create exchange API instances
std::unique_ptr<ApiExchange> createApiExchange(const std::string& exchangeName, OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode); 