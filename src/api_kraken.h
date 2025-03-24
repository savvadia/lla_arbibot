#pragma once

#include "exchange_api.h"
#include "orderbook.h"
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
#include <curl/curl.h>

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

class KrakenApi : public ExchangeApi {
public:
    KrakenApi(OrderBookManager& orderBookManager);
    ~KrakenApi();

    bool connect() override;
    void disconnect() override;
    bool subscribeOrderBook(TradingPair pair) override;
    bool getOrderBookSnapshot(TradingPair pair) override;
    std::string getExchangeName() const override { return "Kraken"; }
    ExchangeId getExchangeId() const override { return ExchangeId::KRAKEN; }
    TradingPair symbolToTradingPair(const std::string& symbol) const override;
    std::string tradingPairToSymbol(TradingPair pair) const override;

private:
    void doConnect();
    void onConnect(beast::error_code ec);
    void doRead();
    void onRead(beast::error_code ec, std::size_t bytes_transferred);
    void doWrite(std::string message);
    void onWrite(beast::error_code ec, std::size_t bytes_transferred);
    void send(std::string message);
    void processMessage(const std::string& message);

    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);

    static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    json makeHttpRequest(const std::string& endpoint, const std::string& params = "");

    OrderBookManager& m_orderBookManager;
    std::map<TradingPair, std::string> m_symbolMap;
    bool m_connected;
    
    net::io_context m_ioc;
    ssl::context m_ctx{ssl::context::tlsv12_client};
    std::unique_ptr<websocket::stream<beast::ssl_stream<beast::tcp_stream>>> m_ws;
    beast::flat_buffer m_buffer;
    std::string m_host{"ws.kraken.com"};
    std::string m_port{"443"};
    
    std::unique_ptr<net::executor_work_guard<net::io_context::executor_type>> m_work;
    std::thread m_thread;
    CURL* curl{nullptr};
}; 