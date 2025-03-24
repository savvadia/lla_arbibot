#include "api_kraken.h"
#include "orderbook.h"
#include "trading_pair_format.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;

// HTTP endpoint
constexpr const char* HTTP_ENDPOINT = "https://api.kraken.com/0";

// HTTP client callback
size_t KrakenApi::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}

// Make HTTP request to Kraken API
json KrakenApi::makeHttpRequest(const std::string& endpoint, const std::string& params) {
    if (!curl) {
        throw std::runtime_error("CURL not initialized");
    }

    std::string url = std::string(HTTP_ENDPOINT) + endpoint;
    if (!params.empty()) {
        url += "?" + params;
    }

    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }

    try {
        return json::parse(response);
    } catch (const std::exception& e) {
        std::cerr << "Failed to parse JSON response: " << e.what() << std::endl;
        return json();
    }
}

// Kraken-specific symbol mapping
TradingPair KrakenApi::symbolToTradingPair(const std::string& symbol) const {
    std::string lowerSymbol = toLower(symbol);
    
    if (lowerSymbol == "xbt/usd" || lowerSymbol == "btc/usd") return TradingPair::BTC_USDT;
    if (lowerSymbol == "eth/usd") return TradingPair::ETH_USDT;
    if (lowerSymbol == "xtz/usd") return TradingPair::XTZ_USDT;
    
    return TradingPair::UNKNOWN;
}

std::string KrakenApi::tradingPairToSymbol(TradingPair pair) const {
    auto it = m_symbolMap.find(pair);
    if (it != m_symbolMap.end()) {
        return it->second;
    }
    throw std::runtime_error("Unsupported trading pair");
}

KrakenApi::KrakenApi(OrderBookManager& orderBookManager)
    : m_orderBookManager(orderBookManager)
    , m_connected(false)
    , curl(nullptr) {
    
    // Initialize CURL
    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    // Initialize symbol map with only BTC, ETH, and XTZ
    m_symbolMap[TradingPair::BTC_USDT] = "XBT/USD";
    m_symbolMap[TradingPair::ETH_USDT] = "ETH/USD";
    m_symbolMap[TradingPair::XTZ_USDT] = "XTZ/USD";
}

KrakenApi::~KrakenApi() {
    disconnect();
    if (curl) {
        curl_easy_cleanup(curl);
    }
}

bool KrakenApi::connect() {
    try {
        m_work = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(m_ioc.get_executor());
        m_thread = std::thread([this]() { m_ioc.run(); });

        doConnect();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error connecting to Kraken: " << e.what() << std::endl;
        return false;
    }
}

void KrakenApi::disconnect() {
    if (m_ws) {
        m_ws->async_close(websocket::close_code::normal,
            [](beast::error_code ec) {
                if (ec) {
                    std::cerr << "Error closing connection: " << ec.message() << std::endl;
                }
            });
    }

    m_work.reset();
    if (m_thread.joinable()) {
        m_thread.join();
    }
    m_connected = false;
}

void KrakenApi::doConnect() {
    tcp::resolver resolver(m_ioc);
    resolver.async_resolve(m_host, m_port,
        [this](beast::error_code ec, tcp::resolver::results_type results) {
            if (ec) {
                std::cerr << "Resolve error: " << ec.message() << std::endl;
                return;
            }

            m_ws = std::make_unique<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(
                m_ioc, m_ctx);

            beast::get_lowest_layer(*m_ws).async_connect(results,
                [this](beast::error_code ec, tcp::endpoint) {
                    if (ec) {
                        std::cerr << "Connect error: " << ec.message() << std::endl;
                        return;
                    }

                    m_ws->next_layer().async_handshake(ssl::stream_base::client,
                        [this](beast::error_code ec) {
                            if (ec) {
                                std::cerr << "SSL handshake error: " << ec.message() << std::endl;
                                return;
                            }

                            m_ws->async_handshake(m_host, "/",
                                [this](beast::error_code ec) {
                                    if (ec) {
                                        std::cerr << "Handshake failed: " << ec.message() << std::endl;
                                        return;
                                    }
                                    m_connected = true;
                                    doRead();
                                });
                        });
                });
        });
}

void KrakenApi::doRead() {
    m_ws->async_read(m_buffer,
        [this](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Read error: " << ec.message() << std::endl;
                return;
            }

            std::string message = beast::buffers_to_string(m_buffer.data());
            m_buffer.consume(m_buffer.size());
            processMessage(message);
            doRead();
        });
}

void KrakenApi::processMessage(const std::string& message) {
    try {
        json data = json::parse(message);

        // Check if it's a heartbeat message
        if (data.contains("event") && data["event"] == "heartbeat") {
            return;
        }

        // Check if it's an order book update
        if (data.contains("channel") && data["channel"] == "book") {
            std::string symbol = data["pair"];
            TradingPair pair = symbolToTradingPair(symbol);
            
            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;

            // Process bid updates
            if (data.contains("b")) {
                for (const auto& bid : data["b"]) {
                    bids.push_back({
                        std::stod(bid[0].get<std::string>()),  // price
                        std::stod(bid[1].get<std::string>())   // quantity
                    });
                }
            }

            // Process ask updates
            if (data.contains("a")) {
                for (const auto& ask : data["a"]) {
                    asks.push_back({
                        std::stod(ask[0].get<std::string>()),  // price
                        std::stod(ask[1].get<std::string>())   // quantity
                    });
                }
            }

            m_orderBookManager.updateOrderBook(pair, bids, asks);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

bool KrakenApi::subscribeOrderBook(TradingPair pair) {
    if (!m_connected) {
        std::cerr << "Not connected to Kraken" << std::endl;
        return false;
    }

    auto it = m_symbolMap.find(pair);
    if (it == m_symbolMap.end()) {
        std::cerr << "Unsupported trading pair" << std::endl;
        return false;
    }

    json subscribeMsg = {
        {"event", "subscribe"},
        {"subscription", {
            {"name", "book"}
        }},
        {"pair", {it->second}}
    };

    doWrite(subscribeMsg.dump());
    return true;
}

void KrakenApi::doWrite(std::string message) {
    m_ws->async_write(net::buffer(message),
        [message](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Write failed: " << ec.message() << std::endl;
            }
        });
}

bool KrakenApi::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        std::cerr << "Not connected to Kraken" << std::endl;
        return false;
    }

    auto it = m_symbolMap.find(pair);
    if (it == m_symbolMap.end()) {
        std::cerr << "Unsupported trading pair" << std::endl;
        return false;
    }

    std::string url = "https://api.kraken.com/0/public/Depth?pair=" + it->second + "&count=100";
    
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size, size_t nmemb, std::string* userp) {
            userp->append((char*)contents, size * nmemb);
            return size * nmemb;
        });

        std::string response;
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        CURLcode res = curl_easy_perform(curl);
        if (res == CURLE_OK) {
            try {
                json data = json::parse(response);
                if (data.contains("result")) {
                    processOrderBookSnapshot(data["result"][it->second], pair);
                    return true;
                }
            } catch (const std::exception& e) {
                std::cerr << "Error parsing snapshot: " << e.what() << std::endl;
            }
        }
    }

    return false;
}

void KrakenApi::processOrderBookSnapshot(const json& data, TradingPair pair) {
    try {
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process asks
        for (const auto& ask : data["asks"]) {
            double price = std::stod(ask[0].get<std::string>());
            double quantity = std::stod(ask[1].get<std::string>());
            asks.push_back({price, quantity});
        }

        // Process bids
        for (const auto& bid : data["bids"]) {
            double price = std::stod(bid[0].get<std::string>());
            double quantity = std::stod(bid[1].get<std::string>());
            bids.push_back({price, quantity});
        }

        m_orderBookManager.updateOrderBook(pair, bids, asks);
    } catch (const std::exception& e) {
        std::cerr << "Error processing order book snapshot: " << e.what() << std::endl;
    }
}

void KrakenApi::send(std::string message) {
    if (!m_connected) {
        std::cerr << "Not connected to Kraken" << std::endl;
        return;
    }
    m_ws->async_write(net::buffer(message),
        [message](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                std::cerr << "Write failed: " << ec.message() << std::endl;
            }
        });
} 