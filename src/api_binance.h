#pragma once

#include "api_exchange.h"
#include "orderbook.h"
#include "trading_pair_format.h"
#include "timers.h"
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>
#include <map>
#include <curl/curl.h>

namespace beast = boost::beast;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

class ApiBinance : public ApiExchange {
public:
    ApiBinance(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode = true);
    ~ApiBinance() override;

    // Initialize WebSocket connection
    bool connect() override;
    void disconnect() override;

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook(std::vector<TradingPair> pairs) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;

    // Process messages for all exchanges
    void processMessages() override;
    void processBookTicker(const json& data);

    // Order management
    bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) override;
    bool cancelOrder(const std::string& orderId) override;
    bool getBalance(const std::string& asset) override;

    std::string getExchangeName() const override { return "BINANCE"; }
    ExchangeId getExchangeId() const override { return ExchangeId::BINANCE; }
    bool isConnected() const override { return m_connected; }

    // Callback setters
    void setSubscriptionCallback(std::function<void(bool)> callback) override;
    void setSnapshotCallback(std::function<void(bool)> callback) override;
    void setOrderCallback(std::function<void(bool)> callback) override;
    void setBalanceCallback(std::function<void(bool)> callback) override;

    // Static method for string conversion (used for tracing)
    static std::string tradingPairToString(TradingPair pair) {
        switch (pair) {
            case TradingPair::BTC_USDT: return "BTCUSDT";
            case TradingPair::ETH_USDT: return "ETHUSDT";
            case TradingPair::XTZ_USDT: return "XTZUSDT";
            default: return "UNKNOWN";
        }
    }

protected:
    // Override the cooldown method for Binance-specific rate limiting
    void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "") override;
    
    // Implement pure virtual methods from base class
    void processRateLimitHeaders(const std::string& headers) override;
    std::string getRestEndpoint() const override;

private:
    struct SymbolState {
        bool subscribed{false};
        bool hasSnapshot{false};
        int64_t lastUpdateId{0};
        bool hasProcessedFirstUpdate{false};  // Track if we've processed the first update after snapshot
    };

    // WebSocket callbacks
    void doRead();
    void processMessage(const std::string& message);
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);
    void doWrite(std::string message);
    void onKeepaliveTimer();
    void doPing();

    // HTTP callbacks
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    static size_t HeaderCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    
    // HTTP request handling
    json makeHttpRequest(const std::string& endpoint, const std::string& params = "", const std::string& method = "GET") override;

    // Internal symbol conversion methods
    TradingPair symbolToTradingPair(const std::string& symbol) const;
    std::string tradingPairToSymbol(TradingPair pair) const;

    std::map<TradingPair, std::string> m_symbolMap;
    bool m_connected;
    bool m_subscribed;
    
    // Callbacks
    std::function<void(bool)> m_subscriptionCallback;
    std::function<void()> m_updateCallback;
    std::function<void(bool)> m_snapshotCallback;
    std::function<void(bool)> m_orderCallback;
    std::function<void(bool)> m_balanceCallback;
    
    net::io_context m_ioc;
    ssl::context m_ctx{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> m_ws;
    beast::flat_buffer m_buffer;
    std::string m_host{"stream.binance.com"};
    std::string m_port{"9443"};  // Using SSL port
    
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_work;
    std::thread m_thread;
    CURL* curl{nullptr};
    std::map<TradingPair, SymbolState> symbolStates;
}; 