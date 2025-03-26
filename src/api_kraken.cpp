#include "api_kraken.h"
#include "orderbook.h"
#include "trading_pair_format.h"
#include <iostream>
#include <sstream>
#include <algorithm>
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
namespace http = boost::beast::http;
namespace ssl = boost::asio::ssl;

constexpr const char* REST_ENDPOINT = "https://api.kraken.com/0";

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

    std::string url = std::string(REST_ENDPOINT) + endpoint;
    if (!params.empty()) {
        url += "?" + params;
    }

    TRACE_BASE(TraceInstance::API, "Making HTTP request to: ", url);

    std::string response;
    
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 2L);
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("curl_easy_perform() failed: ") + curl_easy_strerror(res));
    }

    TRACE_BASE(TraceInstance::API, "Response: ", response);

    if (response.empty()) {
        throw std::runtime_error("Empty response from server");
    }

    return json::parse(response);
}

// Kraken-specific symbol mapping
TradingPair KrakenApi::symbolToTradingPair(const std::string& symbol) const {
    std::string lowerSymbol = symbol;
    std::transform(lowerSymbol.begin(), lowerSymbol.end(), lowerSymbol.begin(), ::tolower);
    
    if (lowerSymbol == "xbtusdt") return TradingPair::BTC_USDT;
    if (lowerSymbol == "ethusdt") return TradingPair::ETH_USDT;
    if (lowerSymbol == "xtzusdt") return TradingPair::XTZ_USDT;
    
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
    m_symbolMap[TradingPair::BTC_USDT] = "XBTUSDT";
    m_symbolMap[TradingPair::ETH_USDT] = "ETHUSDT";
    m_symbolMap[TradingPair::XTZ_USDT] = "XTZUSDT";
}

KrakenApi::~KrakenApi() {
    if (curl) {
        curl_easy_cleanup(curl);
    }
    disconnect();
}

bool KrakenApi::connect() {
    if (m_connected) {
        TRACE_BASE(TraceInstance::API, "Already connected to Kraken");
        return true;
    }

    try {
        // Set up SSL context
        m_ctx.set_verify_mode(ssl::verify_peer);
        m_ctx.set_default_verify_paths();

        // Create the WebSocket stream
        m_ws = std::make_unique<websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>(m_ioc, m_ctx);

        // These two lines are needed for SSL
        if (!SSL_set_tlsext_host_name(m_ws->next_layer().native_handle(), m_host.c_str())) {
            throw boost::beast::system_error(
                boost::beast::error_code(static_cast<int>(::ERR_get_error()),
                                net::error::get_ssl_category()),
                "Failed to set SNI hostname");
        }

        // Look up the domain name
        tcp::resolver resolver(m_ioc);
        auto const results = resolver.resolve(m_host, m_port);

        // Connect to the IP address we get from a lookup
        boost::beast::get_lowest_layer(*m_ws).connect(results);

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

        TRACE_BASE(TraceInstance::API, "Successfully connected to Kraken");
        return true;
    } catch (const std::exception& e) {
        TRACE_BASE(TraceInstance::API, "Error in connect: ", e.what());
        return false;
    }
}

void KrakenApi::doRead() {
    m_ws->async_read(m_buffer,
        [this](boost::beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                TRACE_BASE(TraceInstance::API, "Read error: ", ec.message());
                return;
            }

            std::string message = boost::beast::buffers_to_string(m_buffer.data());
            m_buffer.consume(m_buffer.size());
            TRACE_BASE(TraceInstance::API, "Received message: ", message);
            processMessage(message);
            doRead();
        });
}

void KrakenApi::disconnect() {
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

        // Now it's safe to close the WebSocket
        if (m_ws) {
            boost::beast::error_code ec;
            m_ws->close(websocket::close_code::normal, ec);
            if (ec) {
                TRACE_BASE(TraceInstance::API, "Warning: Error during WebSocket close: ", ec.message());
            }
            m_ws.reset();
        }

        m_connected = false;
        TRACE_BASE(TraceInstance::API, "Disconnected from Kraken");
    } catch (const std::exception& e) {
        TRACE_BASE(TraceInstance::API, "Warning: Error in disconnect: ", e.what());
    }
}

bool KrakenApi::subscribeOrderBook(TradingPair pair) {
    if (!m_connected) {
        TRACE_BASE(TraceInstance::API, "Not connected to Kraken");
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);

        json subscription = {
            {"event", "subscribe"},
            {"pair", {symbol}},
            {"subscription", {
                {"name", "book"},
                {"depth", 100}
            }}
        };

        TRACE_BASE(TraceInstance::API, "Subscribing to Kraken order book for ", symbol);
        doWrite(subscription.dump());
        if (m_subscriptionCallback) {
            m_subscriptionCallback(true);
        }
        return true;
    } catch (const std::exception& e) {
        TRACE_BASE(TraceInstance::API, "Error subscribing to order book: ", e.what());
        if (m_subscriptionCallback) {
            m_subscriptionCallback(false);
        }
        return false;
    }
}

bool KrakenApi::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        TRACE_BASE(TraceInstance::API, "Not connected to Kraken");
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);
        std::string endpoint = "/public/Depth";
        std::string params = "pair=" + symbol + "&count=1000";

        TRACE_BASE(TraceInstance::API, "Getting order book snapshot for ", symbol);
        json response = makeHttpRequest(endpoint, params);
        processOrderBookSnapshot(response, pair);
        return true;
    } catch (const std::exception& e) {
        TRACE_BASE(TraceInstance::API, "Error getting order book snapshot: ", e.what());
        if (m_snapshotCallback) {
            m_snapshotCallback(false);
        }
        return false;
    }
}

void KrakenApi::processMessage(const std::string& message) {
    try {
        json data = json::parse(message);
        
        // Log all messages for debugging
        TRACE_BASE(TraceInstance::API, "Processing message: ", message);
        
        // Check if it's a subscription response
        if (data.is_object() && data.contains("event")) {
            if (data["event"] == "subscriptionStatus") {
                TRACE_BASE(TraceInstance::API, "Subscription status: ", data["status"]);
                return;
            }
            return;
        }

        // Check if it's an order book update (array format)
        if (data.is_array() && data.size() >= 4) {
            const auto& book = data[1];
            std::string pair = data[3];
            std::string channelName = data[2];

            if (channelName.find("book-") == 0) {
                TradingPair tradingPair = symbolToTradingPair(pair);
                if (tradingPair == TradingPair::UNKNOWN) {
                    return;
                }

                std::vector<PriceLevel> bids;
                std::vector<PriceLevel> asks;

                // Process asks
                if (book.contains("as") || book.contains("a")) {
                    const auto& askData = book.contains("as") ? book["as"] : book["a"];
                    for (const auto& ask : askData) {
                        if (ask.size() >= 2) {
                            double price = std::stod(ask[0].get<std::string>());
                            double quantity = std::stod(ask[1].get<std::string>());
                            if (quantity > 0) {
                                asks.push_back({price, quantity});
                            }
                        }
                    }
                }

                // Process bids
                if (book.contains("bs") || book.contains("b")) {
                    const auto& bidData = book.contains("bs") ? book["bs"] : book["b"];
                    for (const auto& bid : bidData) {
                        if (bid.size() >= 2) {
                            double price = std::stod(bid[0].get<std::string>());
                            double quantity = std::stod(bid[1].get<std::string>());
                            if (quantity > 0) {
                                bids.push_back({price, quantity});
                            }
                        }
                    }
                }

                if (!bids.empty() || !asks.empty()) {
                    TRACE_BASE(TraceInstance::API, "Updating order book for ", pair, " with ", bids.size(), " bids and ", asks.size(), " asks");
                    m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, tradingPair, bids, asks);
                }
            }
        }
    } catch (const std::exception& e) {
        TRACE_BASE(TraceInstance::API, "Error processing message: ", e.what());
    }
}

void KrakenApi::processOrderBookUpdate(const json& data) {
    try {
        if (!data.contains("event") || data["event"] != "book") {
            return;
        }

        std::string symbol = data["pair"].get<std::string>();
        TradingPair pair = symbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            return;
        }

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : data["b"]) {
            bids.push_back({
                std::stod(bid[0].get<std::string>()),  // price
                std::stod(bid[1].get<std::string>())   // quantity
            });
        }

        // Process asks
        for (const auto& ask : data["a"]) {
            asks.push_back({
                std::stod(ask[0].get<std::string>()),  // price
                std::stod(ask[1].get<std::string>())   // quantity
            });
        }

        TRACE_BASE(TraceInstance::API, "Updating order book for ", symbol, " with ", bids.size(), " bids and ", asks.size(), " asks");
        m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, bids, asks);
    } catch (const std::exception& e) {
        TRACE_BASE(TraceInstance::API, "Error processing order book update: ", e.what());
    }
}

void KrakenApi::processOrderBookSnapshot(const json& data, TradingPair pair) {
    try {
        auto& state = symbolStates[pair];
        state.hasSnapshot = true;
        
        std::string symbol = tradingPairToSymbol(pair);
        const auto& orderBookData = data["result"][symbol];
        
        // Process bids
        for (const auto& bid : orderBookData["bids"]) {
            double price = std::stod(bid[0].get<std::string>());
            double quantity = std::stod(bid[1].get<std::string>());
            if (quantity > 0) {
                m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, price, quantity, true);
            }
        }
        
        // Process asks
        for (const auto& ask : orderBookData["asks"]) {
            double price = std::stod(ask[0].get<std::string>());
            double quantity = std::stod(ask[1].get<std::string>());
            if (quantity > 0) {
                m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, price, quantity, false);
            }
        }
        
        TRACE_BASE(TraceInstance::API, "Processed order book snapshot for ", tradingPairToSymbol(pair));
        if (m_snapshotCallback) {
            m_snapshotCallback(true);
        }
    } catch (const std::exception& e) {
        TRACE_BASE(TraceInstance::API, "Error processing order book snapshot: ", e.what());
        if (m_snapshotCallback) {
            m_snapshotCallback(false);
        }
    }
}

void KrakenApi::doWrite(std::string message) {
    m_ws->async_write(net::buffer(message),
        [](boost::beast::error_code ec, std::size_t bytes_transferred) {
            if (ec) {
                TRACE_BASE(TraceInstance::API, "Write error: ", ec.message());
                return;
            }
        });
} 