#pragma once

#include <boost/asio/io_context.hpp>
#include <string>
#include <functional>
#include <memory>
#include <chrono>
#include <mutex>
#include <thread>
#include <map>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <boost/asio/strand.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>
#include <queue>

#include "tracer.h"
#include "types.h"
#include "timers.h"

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

// Base class for exchange APIs
class ApiExchange : public Traceable {
public:
    ApiExchange(
        const std::string& restEndpoint,
        const std::string& wsHost, const std::string& wsPort, const std::string& wsEndpoint,
        const std::vector<TradingPair> pairs, bool testMode = true);
    virtual ~ApiExchange();

    // Connect to the exchange
    virtual bool connect();
    virtual void disconnect();

    // Subscribe to order book updates for a trading pair
    virtual bool subscribeOrderBook() = 0;
    virtual bool resubscribeOrderBook(const std::vector<TradingPair>& pairs) = 0;

    // Get order book snapshot for a trading pair
    virtual bool getOrderBookSnapshot(TradingPair pair) = 0;

    // Order management
    virtual bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) = 0;
    virtual bool cancelOrder(const std::string& orderId) = 0;
    virtual bool getBalance(const std::string& asset) = 0;

    // Get exchange name
    virtual std::string getExchangeName() const = 0;

    // Get exchange ID
    virtual ExchangeId getExchangeId() const = 0;

    // Check if connected to the exchange
    bool isConnected() const { return m_connected; }

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
    void setSnapshotCallback(std::function<void(bool)> callback);
    void setOrderCallback(std::function<void(bool)> callback);
    void setBalanceCallback(std::function<void(bool)> callback);

    // Tracing
    void trace(std::ostream& os) const override {}

    // Utility functions
    static std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }

    // Common HTTP request handling
    json makeHttpRequest(const std::string& endpoint, const std::string& params = "", const std::string& method = "GET", bool addJsonHeader = false);
    
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
    
    // Snapshot validity check
    enum class SnapshotRestoring {
        NONE,
        IN_PROGRESS,
    };
    SnapshotRestoring m_snapshotRestoring;
    void startSnapshotValidityTimer(int intervalMs = Config::SNAPSHOT_VALIDITY_CHECK_INTERVAL_MS);
    SnapshotRestoring checkSnapshotValidity();
    static void snapshotValidityCheckCallback(int id, void* data) {
        auto* exchange = static_cast<ApiExchange*>(data);
        exchange->checkSnapshotValidity();
        exchange->startSnapshotValidityTimer();
    }

protected:
    struct SymbolState {
        bool subscribed{false};
        int64_t lastUpdateId{0};
        bool hasProcessedFirstUpdate{false};  // Track if we've processed the first update after snapshot
    private:
        bool m_hasSnapshot{false};
    public:
        bool hasSnapshot() const { return m_hasSnapshot; }
        void setHasSnapshot(bool value) { m_hasSnapshot = value; }
    };

    void doRead();
    void doWrite(std::string message);
    void writeNext();

    // Helper method to set snapshot state and manage timer
    void setSymbolSnapshotState(TradingPair pair, bool hasSnapshot) {
        symbolStates[pair].setHasSnapshot(hasSnapshot);
        if (hasSnapshot) {
            // Restart the snapshot validity check timer when we get a new snapshot
            timersManager.stopTimer(m_snapshotValidityTimerId);
            startSnapshotValidityTimer();
        }
    }

    bool m_connected{false};
    bool m_subscribed{false};
    bool m_testMode;
    bool m_inCooldown;
    std::chrono::steady_clock::time_point m_cooldownEndTime;
    std::mutex m_cooldownMutex;
    std::map<std::string, int> m_rateLimits;
    uint64_t m_snapshotValidityTimerId;  // Timer ID for snapshot validity check
    
    TradingPair symbolToTradingPair(const std::string& symbol) const {
        try {
            return TradingPairData::fromSymbol(getExchangeId(), symbol);
        } catch (const std::exception& e) {
            throw std::runtime_error("Error converting symbol (" + symbol + ") to trading pair: " + std::string(e.what()));
        }
    }

    std::string tradingPairToSymbol(TradingPair pair) const {
        return TradingPairData::getSymbol(getExchangeId(), pair);
    }

    // CURL callback functions
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
    static size_t WriteHeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata);
    std::string formatCurlHeaders(struct curl_slist* headers);
    // Helper methods for derived classes
    virtual void processRateLimitHeaders(const std::string& headers) = 0;

    void processMessage(std::string message);
    virtual void processMessage(const json& data) = 0;
    
    // Callbacks
    std::function<void()> m_updateCallback;
    std::function<void(bool)> m_snapshotCallback;
    std::function<void(bool)> m_orderCallback;
    std::function<void(bool)> m_balanceCallback;

    net::io_context m_ioc;
    ssl::context m_ctx{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> m_ws;
    beast::flat_buffer m_buffer;

    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_work;
    std::thread m_thread;
    CURL* m_curl{nullptr};

    std::map<TradingPair, SymbolState> symbolStates;
    std::string m_restEndpoint;
    std::string m_wsHost;
    std::string m_wsPort;
    std::string m_wsEndpoint;

    std::vector<TradingPair> m_pairs;

    // WebSocket synchronization
    std::mutex m_wsMutex;
    std::queue<std::string> m_writeQueue;
    bool m_isWriting{false};
};

// Factory function to create exchange API instances
std::unique_ptr<ApiExchange> createApiExchange(const ExchangeId exchangeId, const std::vector<TradingPair> pairs, bool testMode); 
