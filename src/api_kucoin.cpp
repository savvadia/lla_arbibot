#include <iostream>
#include <sstream>
#include <regex>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <nlohmann/json.hpp>
#include "api_kucoin.h"
#include "orderbook_mgr.h"
#include "tracer.h"

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;

#define TRACE(...) TRACE_THIS(TraceInstance::A_KUCOIN, ExchangeId::KUCOIN, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_KUCOIN, ExchangeId::KUCOIN, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::A_KUCOIN, ExchangeId::KUCOIN, __VA_ARGS__)

#define ERROR_CNT(_id, ...) ERROR_COUNT(TraceInstance::A_KUCOIN, _id, ExchangeId::KUCOIN, __VA_ARGS__)

constexpr const char* REST_ENDPOINT = "https://api.kucoin.com";

ApiKucoin::ApiKucoin(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
        const std::vector<TradingPair> pairs, bool testMode)
        : ApiExchange(orderBookManager, timersMgr,  
        REST_ENDPOINT,
        "to_be_read_from_rest_endpoint", "443", "to_be_read_from_rest_endpoint",
        pairs, testMode) {
}

// Timer callback for pinging the server to keep the connection alive
// ping message is: { "id": "ping-123", "type": "ping" }

void timerPingCallback(int id, void* data) {
    auto* exchange = static_cast<ApiKucoin*>(data);
    exchange->sendPing();
}

void ApiKucoin::startPingTimer() {
    m_pingTimerId = m_timersMgr.addTimer(m_pingIntervalMs, timerPingCallback, this, TimerType::EXCHANGE_PING);
}

bool ApiKucoin::connect() {
    if (!initWebSocketEndpoint()) {
        ERROR("Failed to initialize KuCoin WebSocket endpoint.");
        return false;
    }
    if (!ApiExchange::connect()) {
        ERROR("Failed to connect to KuCoin");
        return false;
    }
    startPingTimer();
    return true;
}

void ApiKucoin::sendPing() {
    static int pingId = 1;
    doWrite("{\"id\": \"" + std::to_string(pingId++) + "\", \"type\": \"ping\"}");
    startPingTimer();
}

bool ApiKucoin::initWebSocketEndpoint() {
    // Request WebSocket endpoint info
    json response;
    try {
        response = makeHttpRequest("/api/v1/bullet-public", "", "POST", true);
    } catch (const std::exception& ex) {
        TRACE("Failed to get WebSocket endpoint: ", ex.what(), " response: ", response.dump());
        return false;
    }

    DEBUG("Got response: ", response.dump());

    // Validate response
    if (response.is_discarded() || !response.contains("data")) {
        ERROR("Response is discarded or missing 'data' field. ", response.dump());
        return false;
    }

    const auto& data = response["data"];
    if (!data.contains("instanceServers") || !data["instanceServers"].is_array() || data["instanceServers"].empty()) {
        ERROR("No 'instanceServers' array in response. ", response.dump());
        return false;
    }

    const auto& server = data["instanceServers"][0];
    if (!server.contains("endpoint") || !server.contains("pingInterval") || !server.contains("pingTimeout")) {
        ERROR("Missing required fields in 'instanceServers[0]'. ", response.dump());
        return false;
    }

    // Extract values
    m_token = data.value("token", "");
    m_pingIntervalMs = server.value("pingInterval", 0);
    m_pingTimeoutMs = server.value("pingTimeout", 0);
    std::string endpoint = server.value("endpoint", "");

    if (endpoint.empty() || m_token.empty()) {
        ERROR("Endpoint or token is empty. ", response.dump());
        return false;
    }

    // Build WebSocket URL
    std::string wsUrl = endpoint + "?token=" + m_token;

    // Parse wss://host/path
    static const std::regex urlRegex(R"(wss:\/\/([^\/]+)(\/.*))");
    std::smatch match;
    if (std::regex_match(wsUrl, match, urlRegex)) {
        m_wsHost = match[1];
        m_wsPort = "443";
        m_wsEndpoint = match[2];
        TRACE("Got WebSocket endpoint: ", m_wsHost, ":", m_wsPort, " m_wsEndpoint: ", m_wsEndpoint);
        return true;
    }

    ERROR("WebSocket URL did not match expected format: ", wsUrl);
    return false;
}


void ApiKucoin::processMessage(const std::string& message) {
    try {
        DEBUG("Received message: ", message.substr(0, 300));
        json data = json::parse(message);
        
        if (data.contains("e")) {
            std::string eventType = data["e"];
            TRACE("received message type: ", eventType, " ", data.dump().substr(0, 300));
            if (eventType == "depthUpdate") {
                processOrderBookUpdate(data);
            } else if (eventType == "executionReport") {
                ERROR("not implemented: Execution report: ", data.dump());
                // processExecutionReport(data);
            } else {
                ERROR("Unhandled event type: ", eventType);
            }
        } else if (data.contains("b") && data.contains("a") && data.contains("B") && data.contains("A")) {
            DEBUG("received bookTicker: ", data.dump().substr(0, 300));
            processBookTicker(data);
        } else if (data.contains("result") && data["result"] == nullptr) {
            // This is a subscription response
            TRACE("Subscription successful", data.dump());
        } else if (data.contains("id")) {
            // This is a subscription response
            TRACE("Subscription response: ", data.dump());
        } else {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unhandled message type: ", message);
        }
    } catch (const std::exception& e) {
        ERROR("Error processing message: ", e.what());
    }
}

void ApiKucoin::processBookTicker(const json& data) {
    try {
        std::string symbol = data["s"];
        TradingPair pair = symbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            ERROR("Unknown trading pair in bookTicker: ", symbol);
            return;
        }

        double bidPrice = std::stod(data["b"].get<std::string>());
        double bidQuantity = std::stod(data["B"].get<std::string>());
        double askPrice = std::stod(data["a"].get<std::string>());
        double askQuantity = std::stod(data["A"].get<std::string>());
        std::vector<PriceLevel> bids({{bidPrice, bidQuantity}});
        std::vector<PriceLevel> asks({{askPrice, askQuantity}});
        m_orderBookManager.updateOrderBookBestBidAsk(ExchangeId::KUCOIN, pair, bidPrice, bidQuantity, askPrice, askQuantity);
    
    } catch (const std::exception& e) {
        ERROR("Error processing bookTicker: ", data.dump().substr(0, 300), " ", e.what());
    }
}

void ApiKucoin::processOrderBookUpdate(const json& data) {
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
        if (!state.hasSnapshot()) {
            TRACE("No snapshot for ", symbol, " yet, requesting...");
            if (!getOrderBookSnapshot(pair)) {
                ERROR("Failed to get order book snapshot for ", symbol);
                return;
            }
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
            m_orderBookManager.updateOrderBook(ExchangeId::KUCOIN, pair, bids, asks);
        }

        // Mark that we have a snapshot for this symbol
        setSymbolSnapshotState(pair, true);
        TRACE("Got order book snapshot for ", symbol);
    } catch (const std::exception& e) {
        ERROR("Error processing order book update: ", e.what());
    }
}

void ApiKucoin::processOrderBookSnapshot(const json& data, TradingPair pair) {
    try {
        TRACE("Processing order book snapshot for ", tradingPairToSymbol(pair));

        auto& state = symbolStates[pair];
        state.lastUpdateId = data["lastUpdateId"];
        setSymbolSnapshotState(pair, true);
        
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
            m_orderBookManager.updateOrderBook(ExchangeId::KUCOIN, pair, bids, asks);
            
            TRACE("Processed order book snapshot for ", tradingPairToSymbol(pair),
                " last update: ", m_orderBookManager.getOrderBook(ExchangeId::KUCOIN, pair).getLastUpdate());
            if (m_snapshotCallback) {
                m_snapshotCallback(true);
            }

            // Reset subscription state
            setSymbolSnapshotState(pair, true);
            TRACE("Subscription state for ", tradingPairToSymbol(pair), ": subscribed=", state.subscribed, " hasSnapshot=", state.hasSnapshot());
        }
        
    } catch (const std::exception& e) {
        ERROR("Error processing order book snapshot: ", e.what());
        if (m_snapshotCallback) {
            m_snapshotCallback(false);
        }
    }
}

bool ApiKucoin::placeOrder(TradingPair pair, OrderType type, double price, double quantity) {
    if (!m_connected) {
        TRACE("Not connected to Kucoin");
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
        ERROR("Error placing order: ", e.what());
        return false;
    }
}

bool ApiKucoin::cancelOrder(const std::string& orderId) {
    if (!m_connected) {
        TRACE("Not connected to Kucoin");
        return false;
    }

    try {
        std::string params = "orderId=" + orderId;
        json response = makeHttpRequest("/order", params, "DELETE");
        TRACE("Order cancelled successfully: ", response.dump());
        return true;
    } catch (const std::exception& e) {
        ERROR("Error cancelling order: ", e.what());
        return false;
    }
}

bool ApiKucoin::getBalance(const std::string& asset) {
    if (!m_connected) {
        TRACE("Not connected to Kucoin");
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
        ERROR("Error getting balance: ", e.what());
        return false;
    }
}

bool ApiKucoin::subscribeOrderBook() {
    bool success = true;
    if (!m_connected) {
        TRACE("Not connected to Kucoin");
        return false;
    }

    TRACE("Subscribing to Kucoin order book for ", m_pairs.size(), " pairs");

    for (const auto& pair : m_pairs) {

        try {
            json message;
            std::string symbol = tradingPairToSymbol(pair);  
            message["id"] = "arbibot_id";
            message["type"] = "subscribe";
            message["topic"] = "/market/level2:" + symbol;
            message["privateChannel"] = false;
            message["response"] = true;

            TRACE("Subscribing to Kucoin order book with message: ", message.dump());
            doWrite(message.dump());  

            // Store the subscription state
            auto& state = symbolStates[pair];
              
            state.subscribed = true;
            setSymbolSnapshotState(pair, false);
            TRACE("Subscription state for ", symbol, ": subscribed=", state.subscribed, " hasSnapshot=", state.hasSnapshot());
        } catch (const std::exception& e) {
            ERROR("Error subscribing to order book: ", e.what());
            success = false;
        }
    }

    return success;
}

bool ApiKucoin::resubscribeOrderBook(const std::vector<TradingPair>& pairs) {
    if (!m_connected) {
        TRACE("Not connected to Kucoin");
        return false;
    }
    ERROR("Not implemented: resubscribeOrderBook");
    return false;
}

bool ApiKucoin::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        TRACE("Not connected to Kucoin");
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
        ERROR("Error getting order book snapshot: ", e.what());
        if (m_snapshotCallback) {
            m_snapshotCallback(false);
        }
        return false;
    }
} 

// Implement the cooldown method for Kucoin-specific rate limiting
void ApiKucoin::cooldown(int httpCode, const std::string& response, const std::string& endpoint) {
    bool shouldCooldown = false;
    int cooldownMinutes = 5; // Default cooldown time

    // Kucoin-specific rate limit handling
    if (httpCode == 429) {
        // Try to parse the retryAfter field from the response
        try {
            json errorJson = json::parse(response);
            if (errorJson.contains("retryAfter")) {
                int retryAfter = errorJson["retryAfter"].get<int>();
                TRACE("Kucoin rate limit retry after ", retryAfter, " seconds");
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
        TRACE("Kucoin entering cooldown for ", cooldownMinutes, " minutes due to HTTP ", httpCode);
        startCooldown(cooldownMinutes);
    }
}

void ApiKucoin::processRateLimitHeaders(const std::string& headers) {
    // Example header: "x-mbx-used-weight: 10"
    // Parse rate limit headers from Kucoin
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
