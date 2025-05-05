#include "api_binance.h"
#include "orderbook.h"
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
namespace ssl = boost::asio::ssl;

#define TRACE(...) TRACE_THIS(TraceInstance::A_BINANCE, ExchangeId::BINANCE, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_BINANCE, ExchangeId::BINANCE, __VA_ARGS__)

constexpr const char* REST_ENDPOINT = "https://api.binance.com/api/v3";

ApiBinance::ApiBinance(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode)
    : ApiExchange(orderBookManager, timersMgr,  
    "stream.binance.com", "9443", REST_ENDPOINT, "/ws/stream", testMode) {
    // Initialize symbol map with only BTC, ETH, and XTZ
    m_symbolMap[TradingPair::BTC_USDT] = "BTCUSDT";
    m_symbolMap[TradingPair::ETH_USDT] = "ETHUSDT";
    m_symbolMap[TradingPair::XTZ_USDT] = "XTZUSDT";
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
            
            try {
                json data = json::parse(message);
                // Handle both array and single object messages
                if (data.is_array()) {
                    if (!data.empty() && data[0].contains("e")) {
                        TRACE("received message type: ", data[0]["e"].get<std::string>(), " ", data.dump().substr(0, 300));
                    } else {
                        TRACE("received message type: array", data.dump().substr(0, 300));
                    }
                } else if (data.contains("e")) {
                    TRACE("received message type: ", data["e"].get<std::string>());
                } else if (data.contains("b") && data.contains("a") && data.contains("B") && data.contains("A")) {
                    DEBUG("received bookTicker: ", data.dump().substr(0, 300));
                } else {
                    TRACE("ERROR: Parsed WebSocket message type: unknown", data.dump().substr(0, 300));
                }
                processMessage(message);
            } catch (const std::exception& e) {
                TRACE("Error processing message: ", e.what(), " message: ", message.substr(0, 300));
            }
            
            doRead();
        });
}

void ApiBinance::processMessage(const std::string& message) {
    try {
        DEBUG("Received message: ", message.substr(0, 300));
        json data = json::parse(message);
        
        if (data.contains("e")) {
            std::string eventType = data["e"];
            if (eventType == "depthUpdate") {
                processOrderBookUpdate(data);
            } else if (eventType == "executionReport") {
                TRACE("ERROR, not implemented: Execution report: ", data.dump());
                // processExecutionReport(data);
            } else {
                TRACE("ERROR: Unhandled event type: ", eventType);
            }
        } else if (data.contains("result") && data["result"] == nullptr) {
            // This is a subscription response
            TRACE("Subscription successful");
        } else if (data.contains("id") && data["id"] == 1) {
            // This is a subscription response
            TRACE("Subscription successful");
        } else if (data.contains("s") && data.contains("b") && data.contains("B") && data.contains("a") && data.contains("A")) {
            // This is a bookTicker message
            processBookTicker(data);
        } else {
            TRACE("Unhandled message type: ", message);
        }
    } catch (const std::exception& e) {
        TRACE("Error processing message: ", e.what());
    }
}

void ApiBinance::processBookTicker(const json& data) {
    try {
        std::string symbol = data["s"];
        TradingPair pair = symbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            TRACE("ERROR: Unknown trading pair in bookTicker: ", symbol);
            return;
        }

        double bidPrice = std::stod(data["b"].get<std::string>());
        double bidQuantity = std::stod(data["B"].get<std::string>());
        double askPrice = std::stod(data["a"].get<std::string>());
        double askQuantity = std::stod(data["A"].get<std::string>());
        std::vector<PriceLevel> bids({{bidPrice, bidQuantity}});
        std::vector<PriceLevel> asks({{askPrice, askQuantity}});
        m_orderBookManager.updateOrderBookBestBidAsk(ExchangeId::BINANCE, pair, bidPrice, bidQuantity, askPrice, askQuantity);
    
    } catch (const std::exception& e) {
        TRACE("Error processing bookTicker: ", data.dump().substr(0, 300), " ", e.what());
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

        auto& state = symbolStates[pair];
        if (!state.hasSnapshot) {
            TRACE("Skipping update for ", symbol, " - no snapshot yet");
            return;
        }

        // Check if this update is after our last snapshot
        if (data.contains("u")) {
            int64_t updateId = data["u"];
            if (updateId <= state.lastUpdateId) {
                TRACE("Skipping update for ", symbol, " - update ID ", updateId, " is before or equal to last snapshot ID ", state.lastUpdateId);
                return;
            }
            TRACE("Processing update for ", symbol, " - update ID ", updateId, " is after last snapshot ID ", state.lastUpdateId);
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

        // Skip updates that would clear the entire order book
        if (bids.empty() && asks.empty()) {
            TRACE("Skipping empty update for ", symbol);
            return;
        }

        // Check if this update would clear all price levels
        bool allZeroBids = true;
        bool allZeroAsks = true;
        for (const auto& bid : bids) {
            if (bid.quantity > 0) {
                allZeroBids = false;
                break;
            }
        }
        for (const auto& ask : asks) {
            if (ask.quantity > 0) {
                allZeroAsks = false;
                break;
            }
        }

        if (allZeroBids && allZeroAsks) {
            TRACE("Skipping update that would clear all price levels for ", symbol);
            return;
        }

        if (!bids.empty() || !asks.empty()) {
            TRACE("Updating order book for ", symbol, " with ", bids.size(), " bids and ", asks.size(), " asks");
            TRACE("First bid: ", (bids.empty() ? "none" : std::to_string(bids[0].price) + "@" + std::to_string(bids[0].quantity)));
            TRACE("First ask: ", (asks.empty() ? "none" : std::to_string(asks[0].price) + "@" + std::to_string(asks[0].quantity)));
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
        
        TRACE("Processed order book snapshot for ", tradingPairToSymbol(pair),
            " last update: ", m_orderBookManager.getOrderBook(ExchangeId::BINANCE, pair).getLastUpdate());
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

void ApiBinance::processMessages() {
    // All messages are processed asynchronously via WebSocket callbacks
}

bool ApiBinance::subscribeOrderBook(std::vector<TradingPair> pairs) {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }

    try {
        json message;
        message["id"] = 1;
        message["method"] = "SUBSCRIBE";
        message["params"] = json::array();
        for (const auto& pair : pairs) {
            std::string symbol = toLower(tradingPairToSymbol(pair));
            message["params"].emplace_back(symbol + "@bookTicker");
        }

        std::string messageStr = message.dump();

        TRACE("Subscribing to Binance order book with message: ", messageStr);
        doWrite(messageStr);  

        // Store the subscription state
        for (const auto& pair : pairs) {
            auto& state = symbolStates[pair];
            std::string symbol = tradingPairToSymbol(pair);    
            state.subscribed = true;
            state.hasSnapshot = false;
            TRACE("Subscription state for ", symbol, ": subscribed=", state.subscribed, " hasSnapshot=", state.hasSnapshot);
        }
        
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
        std::string params = "symbol=" + symbol + "&limit=10";
        
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

// Implement the cooldown method for Binance-specific rate limiting
void ApiBinance::cooldown(int httpCode, const std::string& response, const std::string& endpoint) {
    bool shouldCooldown = false;
    int cooldownMinutes = 5; // Default cooldown time

    // Binance-specific rate limit handling
    if (httpCode == 429) {
        // Try to parse the retryAfter field from the response
        try {
            json errorJson = json::parse(response);
            if (errorJson.contains("retryAfter")) {
                int retryAfter = errorJson["retryAfter"].get<int>();
                TRACE("Binance rate limit retry after ", retryAfter, " seconds");
                cooldownMinutes = std::max(1, retryAfter / 60);
                shouldCooldown = true;
            }
        } catch (...) {
            // If we can't parse the retryAfter, use default cooldown
            shouldCooldown = true;
            cooldownMinutes = 30; // Default for rate limits
        }
    } else if (httpCode == 418) {
        // IP has been auto-banned
        shouldCooldown = true;
        cooldownMinutes = 120; // 2 hours cooldown for bans
    } else if (httpCode == 403) {
        // Authentication issues
        shouldCooldown = true;
        cooldownMinutes = 60; // 1 hour cooldown for auth issues
    } else if (httpCode >= 500) {
        // Server errors
        shouldCooldown = true;
        cooldownMinutes = 15; // 15 minutes for server errors
    } else if (httpCode >= 400 && httpCode < 500) {
        // Client errors
        shouldCooldown = true;
        cooldownMinutes = 10; // 10 minutes for client errors
    }

    // Check for rate limit headers in the response
    // Note: This would be implemented in makeHttpRequest when processing the response
    // Here we're just handling the cooldown based on the HTTP code

    if (shouldCooldown) {
        TRACE("Binance entering cooldown for ", cooldownMinutes, " minutes due to HTTP ", httpCode);
        startCooldown(cooldownMinutes);
    }
}

void ApiBinance::processRateLimitHeaders(const std::string& headers) {
    // Example header: "x-mbx-used-weight: 10"
    // Parse rate limit headers from Binance
    size_t pos = headers.find("x-mbx-used-weight:");
    if (pos != std::string::npos) {
        try {
            std::string value = headers.substr(pos + 17); // Skip "x-mbx-used-weight:"
            int usedWeight = std::stoi(value);
            // Update rate limit info
            updateRateLimit("weight", 1200, 1200 - usedWeight, 60);
        } catch (const std::exception& e) {
            TRACE("Failed to parse rate limit header: ", e.what());
        }
    }
}
