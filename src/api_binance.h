#pragma once

#include "exchange_api.h"
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

class BinanceApi : public ExchangeApi {
public:
    BinanceApi(OrderBookManager& orderBookManager);
    ~BinanceApi();

    bool connect() override;
    void disconnect() override;
    bool subscribeOrderBook(TradingPair pair) override;
    bool getOrderBookSnapshot(TradingPair pair) override;
    std::string getExchangeName() const override { return "Binance"; }
    ExchangeId getExchangeId() const override { return ExchangeId::BINANCE; }
    TradingPair symbolToTradingPair(const std::string& symbol) const override;
    std::string tradingPairToSymbol(TradingPair pair) const override;

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
    void doConnect();
    void onConnect(beast::error_code ec);
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void doWrite(std::string message);
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);
    void processMessage(const std::string& message);

    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);

    // HTTP client methods
    json makeHttpRequest(const std::string& endpoint, const std::string& params = "");
    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    // Track last update ID for each symbol
    struct SymbolState {
        uint64_t lastUpdateId{0};
        bool hasSnapshot{false};
    };
    std::map<TradingPair, SymbolState> symbolStates;

    OrderBookManager& m_orderBookManager;
    std::map<TradingPair, std::string> m_symbolMap;
    bool m_connected;
    
    net::io_context m_ioc;
    ssl::context m_ctx{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> m_ws;
    beast::flat_buffer m_buffer;
    std::string m_host{"stream.binance.com"};
    std::string m_port{"9443"};
    
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_work;
    std::thread m_thread;
    CURL* curl{nullptr};
}; 