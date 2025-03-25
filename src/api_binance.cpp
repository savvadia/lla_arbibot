#include "api_binance.h"
#include "orderbook.h"
#include "trading_pair_format.h"
#include <iostream>
#include <sstream>
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

constexpr const char* REST_ENDPOINT = "https://api.binance.com/api/v3";

// HTTP client callback
size_t BinanceApi::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}

// Make HTTP request to Binance API
json BinanceApi::makeHttpRequest(const std::string& endpoint, const std::string& params) {
    if (!curl) {
        throw std::runtime_error("CURL not initialized");
    }

    std::string url = std::string(REST_ENDPOINT) + endpoint;
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

    return json::parse(response);
}

// Binance-specific symbol mapping
TradingPair BinanceApi::symbolToTradingPair(const std::string& symbol) const {
    std::string lowerSymbol = toLower(symbol);
    
    if (lowerSymbol == "btcusdt") return TradingPair::BTC_USDT;
    if (lowerSymbol == "ethusdt") return TradingPair::ETH_USDT;
    if (lowerSymbol == "xtzusdt") return TradingPair::XTZ_USDT;
    
    return TradingPair::UNKNOWN;
}

std::string BinanceApi::tradingPairToSymbol(TradingPair pair) const {
    auto it = m_symbolMap.find(pair);
    if (it != m_symbolMap.end()) {
        return it->second;
    }
    throw std::runtime_error("Unsupported trading pair");
}

BinanceApi::BinanceApi(OrderBookManager& orderBookManager)
    : m_orderBookManager(orderBookManager)
    , m_connected(false)
    , curl(nullptr) {
    
    // Initialize CURL
    curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    // Initialize symbol map with only BTC, ETH, and XTZ
    m_symbolMap[TradingPair::BTC_USDT] = "BTCUSDT";
    m_symbolMap[TradingPair::ETH_USDT] = "ETHUSDT";
    m_symbolMap[TradingPair::XTZ_USDT] = "XTZUSDT";
}

BinanceApi::~BinanceApi() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
    disconnect();
}

bool BinanceApi::connect() {
    if (m_connected) {
        return true;
    }

    try {
        // Set up SSL context
        m_ctx.set_verify_mode(ssl::verify_peer);
        m_ctx.set_default_verify_paths();

        // Create the WebSocket stream
        m_ws = std::make_unique<websocket::stream<beast::ssl_stream<beast::tcp_stream>>>(m_ioc, m_ctx);

        // These two lines are needed for SSL
        if (!SSL_set_tlsext_host_name(m_ws->next_layer().native_handle(), m_host.c_str())) {
            throw beast::system_error(
                beast::error_code(static_cast<int>(::ERR_get_error()),
                                net::error::get_ssl_category()),
                "Failed to set SNI hostname");
        }

        // Look up the domain name
        auto const results = net::ip::tcp::resolver(m_ioc).resolve(m_host, m_port);

        // Connect to the IP address we get from a lookup
        beast::get_lowest_layer(*m_ws).connect(results);

        // Perform the SSL handshake
        m_ws->next_layer().handshake(ssl::stream_base::client);

        // Perform the websocket handshake
        m_ws->handshake(m_host, "/");

        // Start the IO context in a separate thread
        m_work = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(m_ioc.get_executor());
        m_thread = std::thread([this]() { m_ioc.run(); });

        m_connected = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in connect: " << e.what() << std::endl;
        return false;
    }
}

void BinanceApi::disconnect() {
    if (!m_connected) {
        return;
    }

    try {
        if (m_ws) {
            m_ws->close(websocket::close_code::normal);
        }
        
        if (m_work) {
            m_work.reset();
        }
        
        if (m_thread.joinable()) {
            m_thread.join();
        }
        
        m_ws.reset();
        m_connected = false;
    } catch (const std::exception& e) {
        std::cerr << "Error in disconnect: " << e.what() << std::endl;
    }
}

bool BinanceApi::subscribeOrderBook(TradingPair pair) {
    if (!m_connected) {
        std::cerr << "Not connected to Binance" << std::endl;
        return false;
    }

    auto it = m_symbolMap.find(pair);
    if (it == m_symbolMap.end()) {
        std::cerr << "Unsupported trading pair: " << trading::toString(pair) << std::endl;
        return false;
    }

    try {
        std::string symbol = it->second;
        std::transform(symbol.begin(), symbol.end(), symbol.begin(), ::tolower);

        json subscription = {
            {"method", "SUBSCRIBE"},
            {"params", {symbol + "@depth@100ms"}},
            {"id", 1}
        };

        std::string message = subscription.dump();
        m_ws->write(net::buffer(message));
        if (m_subscriptionCallback) {
            m_subscriptionCallback(true);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in subscribeOrderBook: " << e.what() << std::endl;
        return false;
    }
}

bool BinanceApi::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        std::cerr << "Not connected to Binance" << std::endl;
        return false;
    }

    auto it = m_symbolMap.find(pair);
    if (it == m_symbolMap.end()) {
        std::cerr << "Unsupported trading pair: " << trading::toString(pair) << std::endl;
        return false;
    }

    if (!curl) {
        return false;
    }

    try {
        std::string symbol = it->second;
        std::string endpoint = "/api/v3/depth";
        std::string params = "symbol=" + symbol + "&limit=1000";

        json response = makeHttpRequest(endpoint, params);
        if (response.contains("code")) {
            std::cerr << "Binance API error: " << response["msg"].get<std::string>() << std::endl;
            return false;
        }

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : response["bids"]) {
            bids.push_back({
                std::stod(bid[0].get<std::string>()),  // price
                std::stod(bid[1].get<std::string>())   // quantity
            });
        }

        // Process asks
        for (const auto& ask : response["asks"]) {
            asks.push_back({
                std::stod(ask[0].get<std::string>()),  // price
                std::stod(ask[1].get<std::string>())   // quantity
            });
        }

        m_orderBookManager.updateOrderBook(pair, bids, asks);
        if (m_snapshotCallback) {
            m_snapshotCallback(true);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in getOrderBookSnapshot: " << e.what() << std::endl;
        return false;
    }
}

void BinanceApi::processMessage(const std::string& message) {
    try {
        json data = json::parse(message);

        // Check if it's a heartbeat message
        if (data.contains("result") && data["result"] == nullptr) {
            return;
        }

        // Check if it's an order book update
        if (data.contains("e") && data["e"] == "depthUpdate") {
            std::string symbol = data["s"];
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
            if (m_updateCallback) {
                m_updateCallback();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
}

void BinanceApi::processOrderBookUpdate(const json& data) {
    // ... existing code ...
    if (m_updateCallback) {
        m_updateCallback();
    }
}

void BinanceApi::processOrderBookSnapshot(const json& data, TradingPair pair) {
    // ... existing code ...
    if (m_snapshotCallback) {
        m_snapshotCallback(true);
    }
} 