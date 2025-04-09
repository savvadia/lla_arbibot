#pragma once

#include "api_exchange.h"
#include "orderbook.h"
#include "timers.h"
#include <string>
#include <functional>
#include <memory>
#include <thread>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <map>

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace ssl = boost::asio::ssl;
namespace net = boost::asio;
namespace beast = boost::beast;

class ApiKraken : public ApiExchange {
public:
    ApiKraken(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode = true);
    ~ApiKraken() override;

    // Initialize WebSocket connection
    bool connect() override;
    void disconnect() override;

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook(std::vector<TradingPair> pairs) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;

    // Process incoming messages
    void processMessages() override;

    // Order management
    bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) override;
    bool cancelOrder(const std::string& orderId) override;
    bool getBalance(const std::string& asset) override;

    std::string getExchangeName() const override { return "Kraken"; }
    ExchangeId getExchangeId() const override { return ExchangeId::KRAKEN; }
    bool isConnected() const override { return m_connected; }

    // Callback setters
    void setSubscriptionCallback(std::function<void(bool)> callback) override;
    void setSnapshotCallback(std::function<void(bool)> callback) override;
    void setOrderCallback(std::function<void(bool)> callback) override;
    void setBalanceCallback(std::function<void(bool)> callback) override;

    // Static method for string conversion (used for tracing)
    static std::string tradingPairToString(TradingPair pair) {
        switch (pair) {
            case TradingPair::BTC_USDT: return "XBT/USD";
            case TradingPair::ETH_USDT: return "ETH/USD";
            case TradingPair::XTZ_USDT: return "XTZ/USD";
            default: return "UNKNOWN";
        }
    }

protected:
    // Override the cooldown method for Kraken-specific rate limiting
    void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "") override;

private:
    struct SymbolState {
        bool subscribed{false};
        bool hasSnapshot{false};
    };

    // HTTP client callback
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    json makeHttpRequest(const std::string& endpoint, const std::string& params = "", const std::string& method = "GET");
    
    // WebSocket callbacks
    void doRead();
    void processMessage(const std::string& message);
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);
    void doWrite(std::string message);

    // Internal symbol conversion methods
    TradingPair symbolToTradingPair(const std::string& symbol) const;
    std::string tradingPairToSymbol(TradingPair pair) const;

    std::map<TradingPair, std::string> m_symbolMap;
    bool m_connected;
    
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
    std::string m_host{"ws.kraken.com"};
    std::string m_port{"443"};  // Using SSL port
    
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_work;
    std::thread m_thread;
    CURL* curl{nullptr};
    std::map<TradingPair, SymbolState> symbolStates;
}; 