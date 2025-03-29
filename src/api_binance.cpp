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
#include "tracer.h"

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;
namespace websocket = boost::beast::websocket;
namespace http = beast::http;
namespace ssl = boost::asio::ssl;

constexpr const char* REST_ENDPOINT = "https://api.binance.com/api/v3";

#define TRACE(...) TRACE_THIS(TraceInstance::A_BINANCE, __VA_ARGS__)

// HTTP client callback
size_t ApiBinance::WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    size_t realsize = size * nmemb;
    userp->append((char*)contents, realsize);
    return realsize;
}

// Make HTTP request to Binance API
json ApiBinance::makeHttpRequest(const std::string& endpoint, const std::string& params, const std::string& method) {
    if (!curl) {
        throw std::runtime_error("CURL not initialized");
    }

    std::string url = std::string(REST_ENDPOINT) + endpoint;
    if (!params.empty()) {
        url += "?" + params;
    }

    TRACE("Making HTTP ", method, " request to: ", url);

    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    if (method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!params.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, params.c_str());
        }
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }

    // Truncate long responses for logging
    std::string logResponse = response;
    if (logResponse.length() > 500) {
        logResponse = logResponse.substr(0, 497) + "...";
    }
    TRACE("Response: ", logResponse);

    if (response.empty()) {
        throw std::runtime_error("Empty response from server");
    }

    return json::parse(response);
}

// Binance-specific symbol mapping
TradingPair ApiBinance::symbolToTradingPair(const std::string& symbol) const {
    std::string lowerSymbol = toLower(symbol);
    
    if (lowerSymbol == "btcusdt") return TradingPair::BTC_USDT;
    if (lowerSymbol == "ethusdt") return TradingPair::ETH_USDT;
    if (lowerSymbol == "xtzusdt") return TradingPair::XTZ_USDT;
    
    return TradingPair::UNKNOWN;
}

std::string ApiBinance::tradingPairToSymbol(TradingPair pair) const {
    auto it = m_symbolMap.find(pair);
    if (it != m_symbolMap.end()) {
        return it->second;
    }
    throw std::runtime_error("Unsupported trading pair");
}

ApiBinance::ApiBinance(OrderBookManager& orderBookManager)
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

ApiBinance::~ApiBinance() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
    disconnect();
}

bool ApiBinance::connect() {
    if (m_connected) {
        TRACE("Already connected to Binance");
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
        tcp::resolver resolver(m_ioc);
        auto const results = resolver.resolve(m_host, m_port);

        // Connect to the IP address we get from a lookup
        beast::get_lowest_layer(*m_ws).connect(results);

        // Perform the SSL handshake
        m_ws->next_layer().handshake(ssl::stream_base::client);

        // Perform the websocket handshake
        m_ws->handshake(m_host, "/ws");

        m_connected = true;

        // Start the IO context in a separate thread for reading
        m_work = std::make_unique<net::executor_work_guard<net::io_context::executor_type>>(m_ioc.get_executor());
        m_thread = std::thread([this]() { m_ioc.run(); });

        // Start reading
        doRead();

        TRACE("Successfully connected to Binance");
        return true;
    } catch (const std::exception& e) {
        TRACE("Error in connect: ", e.what());
        return false;
    }
}

void ApiBinance::doRead() {
    m_ws->async_read(m_buffer,
        [this](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                TRACE("Read error: ", ec.message());
                // Try to reconnect on error
                if (m_connected) {
                    TRACE("Attempting to reconnect to Binance...");
                    disconnect();
                    connect();
                }
                return;
            }

            std::string message = beast::buffers_to_string(m_buffer.data());
            m_buffer.consume(m_buffer.size());
            
            // Log the raw message for debugging
            TRACE("Raw WebSocket message: ", message);
            
            try {
                processMessage(message);
            } catch (const std::exception& e) {
                TRACE("Error processing message: ", e.what());
            }
            
            doRead();
        });
}

void ApiBinance::disconnect() {
    if (!m_connected) {
        return;
    }

    try {
        // First, stop the IO context to prevent new operations
        if (m_work) {
            m_work.reset();
        }

        // Stop the IO context
        m_ioc.stop();

        // Wait for the IO thread to finish
        if (m_thread.joinable()) {
            m_thread.join();
        }

        // Reset the IO context for potential reconnection
        m_ioc.restart();

        // Now it's safe to close the WebSocket
        if (m_ws) {
            boost::beast::error_code ec;
            m_ws->close(websocket::close_code::normal, ec);
            if (ec) {
                TRACE("Warning: Error during WebSocket close: ", ec.message());
            }
            m_ws.reset();
        }

        m_connected = false;
        TRACE("Disconnected from Binance");
    } catch (const std::exception& e) {
        TRACE("Warning: Error in disconnect: ", e.what());
    }
}

bool ApiBinance::subscribeOrderBook(TradingPair pair) {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);
        std::stringstream ss;
        // Changed from @depth@100ms to @depth@100ms@1000 to get more frequent updates
        ss << "{\"method\":\"SUBSCRIBE\",\"params\":[\"" << symbol << "@depth@100ms@1000\"],\"id\":1}";
        
        TRACE("Subscribing to Binance order book for ", symbol);
        doWrite(ss.str());
        
        // Store the subscription state
        auto& state = symbolStates[pair];
        state.subscribed = true;
        
        if (m_subscriptionCallback) {
            m_subscriptionCallback(true);
        }
        
        return true;
    } catch (const std::exception& e) {
        TRACE("Error subscribing to order book: ", e.what());
        if (m_subscriptionCallback) {
            m_subscriptionCallback(false);
        }
        return false;
    }
}

bool ApiBinance::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);
        std::string endpoint = "/depth";
        std::string params = "symbol=" + symbol + "&limit=1000";
        
        TRACE("Getting order book snapshot for ", symbol);
        json response = makeHttpRequest(endpoint, params);
        processOrderBookSnapshot(response, pair);
        
        return true;
    } catch (const std::exception& e) {
        TRACE("Error getting order book snapshot: ", e.what());
        if (m_snapshotCallback) {
            m_snapshotCallback(false);
        }
        return false;
    }
}

void ApiBinance::processMessage(const std::string& message) {
    try {
        json data = json::parse(message);
        
        // Log all messages for debugging
        TRACE("Processing message: ", message);
        
        // Check if it's a subscription response
        if (data.contains("result") && data["result"] == nullptr) {
            TRACE("Subscription successful");
            return;
        }

        // Check if it's an order book update
        if (data.contains("e") && data["e"] == "depthUpdate") {
            std::string symbol = data["s"].get<std::string>();
            TradingPair pair = symbolToTradingPair(symbol);
            if (pair == TradingPair::UNKNOWN) {
                TRACE("Unknown trading pair in update: ", symbol);
                return;
            }

            auto& state = symbolStates[pair];
            if (!state.hasSnapshot) {
                TRACE("Skipping update for ", symbol, " - no snapshot yet");
                return;
            }

            // Check if this update is after our last snapshot
            if (data.contains("u") && data["u"] <= state.lastUpdateId) {
                TRACE("Skipping update for ", symbol, " - update ID ", data["u"], " is before or equal to last snapshot ID ", state.lastUpdateId);
                return;
            }

            TRACE("Processing order book update for ", symbol);
            processOrderBookUpdate(data);
        }
    } catch (const std::exception& e) {
        TRACE("Error processing message: ", e.what());
    }
}

void ApiBinance::processOrderBookUpdate(const json& data) {
    try {
        if (!data.contains("e") || data["e"] != "depthUpdate") {
            return;
        }

        std::string symbol = data["s"].get<std::string>();
        TradingPair pair = symbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            TRACE("Unknown trading pair in update: ", symbol);
            return;
        }

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : data["b"]) {
            double price = std::stod(bid[0].get<std::string>());
            double quantity = std::stod(bid[1].get<std::string>());
            if (quantity > 0) {
                bids.push_back({price, quantity});
            } else {
                // If quantity is 0, it's a delete - we'll pass it through with 0 quantity
                bids.push_back({price, 0});
            }
        }

        // Process asks
        for (const auto& ask : data["a"]) {
            double price = std::stod(ask[0].get<std::string>());
            double quantity = std::stod(ask[1].get<std::string>());
            if (quantity > 0) {
                asks.push_back({price, quantity});
            } else {
                // If quantity is 0, it's a delete - we'll pass it through with 0 quantity
                asks.push_back({price, 0});
            }
        }

        if (!bids.empty() || !asks.empty()) {
            TRACE("Updating order book for ", symbol, " with ", bids.size(), " bids and ", asks.size(), " asks");
            m_orderBookManager.updateOrderBook(ExchangeId::BINANCE, pair, bids, asks);
        }
    } catch (const std::exception& e) {
        TRACE("Error processing order book update: ", e.what());
    }
}

void ApiBinance::processOrderBookSnapshot(const json& data, TradingPair pair) {
    try {
        TRACE("Processing order book snapshot for ", tradingPairToSymbol(pair));

        auto& state = symbolStates[pair];
        state.lastUpdateId = data["lastUpdateId"];
        state.hasSnapshot = true;
        
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;
        
        // Process bids
        for (const auto& bid : data["bids"]) {
            double price = std::stod(bid[0].get<std::string>());
            double quantity = std::stod(bid[1].get<std::string>());
            if (quantity > 0) {
                bids.push_back({price, quantity});
            }
        }
        
        // Process asks
        for (const auto& ask : data["asks"]) {
            double price = std::stod(ask[0].get<std::string>());
            double quantity = std::stod(ask[1].get<std::string>());
            if (quantity > 0) {
                asks.push_back({price, quantity});
            }
        }

        // Update the order book with all bids and asks at once
        if (!bids.empty() || !asks.empty()) {
            TRACE("Updating order book for ", tradingPairToSymbol(pair), " with ", bids.size(), " bids and ", asks.size(), " asks");
            m_orderBookManager.updateOrderBook(ExchangeId::BINANCE, pair, bids, asks);
        }
        
        TRACE("Processed order book snapshot for ", tradingPairToSymbol(pair));
        if (m_snapshotCallback) {
            m_snapshotCallback(true);
        }
    } catch (const std::exception& e) {
        TRACE("Error processing order book snapshot: ", e.what());
        if (m_snapshotCallback) {
            m_snapshotCallback(false);
        }
    }
}

bool ApiBinance::placeOrder(TradingPair pair, OrderType type, double price, double quantity) {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);
        std::string side = (type == OrderType::BUY) ? "BUY" : "SELL";
        
        std::stringstream ss;
        ss << "symbol=" << symbol
           << "&side=" << side
           << "&type=LIMIT"
           << "&timeInForce=GTC"
           << "&quantity=" << std::fixed << std::setprecision(8) << quantity
           << "&price=" << std::fixed << std::setprecision(8) << price;

        json response = makeHttpRequest("/order", ss.str());
        TRACE("Order placed successfully: ", response.dump());
        return true;
    } catch (const std::exception& e) {
        TRACE("Error placing order: ", e.what());
        return false;
    }
}

bool ApiBinance::cancelOrder(const std::string& orderId) {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }

    try {
        std::string params = "orderId=" + orderId;
        json response = makeHttpRequest("/order", params, "DELETE");
        TRACE("Order cancelled successfully: ", response.dump());
        return true;
    } catch (const std::exception& e) {
        TRACE("Error cancelling order: ", e.what());
        return false;
    }
}

bool ApiBinance::getBalance(const std::string& asset) {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }

    try {
        json response = makeHttpRequest("/account", "");
        
        for (const auto& balance : response["balances"]) {
            if (balance["asset"] == asset) {
                TRACE("Balance for ", asset, ": Free=", balance["free"].get<std::string>(),
                          ", Locked=", balance["locked"].get<std::string>());
                return true;
            }
        }
        
        TRACE("No balance found for asset: ", asset);
        return false;
    } catch (const std::exception& e) {
        TRACE("Error getting balance: ", e.what());
        return false;
    }
}

void ApiBinance::setSubscriptionCallback(std::function<void(bool)> callback) {
    m_subscriptionCallback = callback;
}

void ApiBinance::setSnapshotCallback(std::function<void(bool)> callback) {
    m_snapshotCallback = callback;
}

void ApiBinance::doWrite(std::string message) {
    TRACE("Sending WebSocket message: ", message);
    m_ws->async_write(net::buffer(message),
        [this](beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                TRACE("Write error: ", ec.message());
                return;
            }
            TRACE("Successfully sent WebSocket message");
        });
} 