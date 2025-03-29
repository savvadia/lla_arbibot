#pragma once

#include "api_exchange.h"
#include "orderbook.h"
#include "trading_pair_format.h"
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
#include <atomic>
#include <algorithm>
#include <curl/curl.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

class ApiBinance : public ApiExchange {
public:
    ApiBinance(OrderBookManager& orderBookManager);
    ~ApiBinance() override;

    // Initialize WebSocket connection
    bool connect() override;
    void disconnect() override;

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook(TradingPair pair) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;

    // Order management
    bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) override;
    bool cancelOrder(const std::string& orderId) override;
    bool getBalance(const std::string& asset) override;

    std::string getExchangeName() const override { return "Binance"; }
    ExchangeId getExchangeId() const override { return ExchangeId::BINANCE; }
    bool isConnected() const override { return m_connected; }

    // Callback setters
    void setSubscriptionCallback(std::function<void(bool)> callback) override;
    void setSnapshotCallback(std::function<void(bool)> callback) override;

    // Static method for string conversion (used for tracing)
    static std::string tradingPairToString(TradingPair pair) {
        switch (pair) {
            case TradingPair::BTC_USDT: return "BTCUSDT";
            case TradingPair::ETH_USDT: return "ETHUSDT";
            case TradingPair::XTZ_USDT: return "XTZUSDT";
            default: return "UNKNOWN";
        }
    }

private:
    struct SymbolState {
        bool hasSnapshot = false;
        bool subscribed = false;
        int64_t lastUpdateId = 0;
    };

    // HTTP client methods
    json makeHttpRequest(const std::string& endpoint, const std::string& params = "", const std::string& method = "GET");
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    // WebSocket callbacks
    void doRead();
    void processMessage(const std::string& message);
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);
    void doWrite(std::string message);

    // Internal symbol conversion methods
    TradingPair symbolToTradingPair(const std::string& symbol) const;
    std::string tradingPairToSymbol(TradingPair pair) const;

    OrderBookManager& m_orderBookManager;
    std::map<TradingPair, std::string> m_symbolMap;
    bool m_connected;
    
    // Callbacks
    std::function<void(bool)> m_subscriptionCallback;
    std::function<void()> m_updateCallback;
    std::function<void(bool)> m_snapshotCallback;
    
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