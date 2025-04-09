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

using json = nlohmann::json;

// Base class for exchange APIs
class ApiExchange : public Traceable {
public:
    ApiExchange(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode = true) : 
        m_testMode(testMode), m_inCooldown(false), m_cooldownEndTime(std::chrono::steady_clock::now()), m_timersMgr(timersMgr), m_orderBookManager(orderBookManager), m_keepaliveTimerId(0) {}
    virtual ~ApiExchange() = default;

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

    // Callback setters
    virtual void setSubscriptionCallback(std::function<void(bool)> callback) = 0;
    virtual void setSnapshotCallback(std::function<void(bool)> callback) = 0;
    virtual void setOrderCallback(std::function<void(bool)> callback) = 0;
    virtual void setBalanceCallback(std::function<void(bool)> callback) = 0;

    // operator<<
    friend std::ostream& operator<<(std::ostream& os, const ApiExchange& api) {
        api.trace(os);
        return os;
    }

    void trace(std::ostream& os) const override {
        os << "Api" << getExchangeName();
    }

protected:
    bool m_testMode;
    std::atomic<bool> m_inCooldown;
    std::chrono::steady_clock::time_point m_cooldownEndTime;
    std::mutex m_cooldownMutex;
    std::map<std::string, int> m_rateLimits; // Track rate limits by endpoint
    TimersMgr& m_timersMgr;
    OrderBookManager& m_orderBookManager;
    int m_keepaliveTimerId;

    // Helper function to convert string to lowercase
    static std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    // Virtual method for exchange-specific cooldown handling
    virtual void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "") {
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

    // Common HTTP error handling
    virtual void handleHttpError(int httpCode, const std::string& response, const std::string& endpoint = "");

    // Start a cooldown period
    virtual void startCooldown(int minutes);

    // End cooldown period
    virtual void endCooldown();

    // Check if cooldown has expired
    virtual bool checkCooldownExpired();

    // Update rate limit tracking
    virtual void updateRateLimit(const std::string& endpoint, int limit, int remaining, int reset);
};

// Factory function to create exchange API instances
std::unique_ptr<ApiExchange> createApiExchange(const std::string& exchangeName, OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode); 