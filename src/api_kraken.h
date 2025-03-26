#pragma once

#include "exchange_api.h"
#include "orderbook.h"
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
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;
namespace net = boost::asio;
namespace beast = boost::beast;

class KrakenApi : public ExchangeApi {
public:
    KrakenApi(OrderBookManager& orderBookManager);
    ~KrakenApi() override;

    // Initialize WebSocket connection
    bool connect() override;
    void disconnect() override;

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook(TradingPair pair) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;

    std::string getExchangeName() const override { return "Kraken"; }
    ExchangeId getExchangeId() const override { return ExchangeId::KRAKEN; }
    TradingPair symbolToTradingPair(const std::string& symbol) const override;
    std::string tradingPairToSymbol(TradingPair pair) const override;
    bool isConnected() const override { return m_connected; }

    // Callback setters
    void setSubscriptionCallback(std::function<void(bool)> callback) { m_subscriptionCallback = callback; }
    void setUpdateCallback(std::function<void()> callback) { m_updateCallback = callback; }
    void setSnapshotCallback(std::function<void(bool)> callback) { m_snapshotCallback = callback; }

private:
    struct SymbolState {
        bool hasSnapshot = false;
    };

    // HTTP client callback
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    // Make HTTP request to Kraken API
    json makeHttpRequest(const std::string& endpoint, const std::string& params);

    // WebSocket callbacks
    void doRead();
    void processMessage(const std::string& message);

    // Process order book updates
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);

    // Convert Kraken symbol to our TradingPair
    TradingPair krakenSymbolToTradingPair(const std::string& symbol);

    OrderBookManager& m_orderBookManager;
    net::io_context m_ioc;
    ssl::context m_ctx{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>> m_ws;
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_work;
    std::thread m_thread;
    boost::beast::flat_buffer m_buffer;
    bool m_connected{false};
    std::map<TradingPair, std::string> m_symbolMap;
    std::string m_host{"ws.kraken.com"};
    std::string m_port{"443"};
    CURL* curl;
    std::map<TradingPair, SymbolState> symbolStates;

    // Callbacks
    std::function<void(bool)> m_subscriptionCallback;
    std::function<void()> m_updateCallback;
    std::function<void(bool)> m_snapshotCallback;

    void doWrite(std::string message);
}; 