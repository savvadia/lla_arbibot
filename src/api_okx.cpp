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

#include "api_okx.h"
#include "orderbook_mgr.h"
#include "tracer.h"

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;

#define TRACE(...) TRACE_THIS(TraceInstance::A_OKX, ExchangeId::OKX, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_OKX, ExchangeId::OKX, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::A_OKX, ExchangeId::OKX, __VA_ARGS__)

#define ERROR_CNT(_id, ...) ERROR_COUNT(TraceInstance::A_OKX, _id, ExchangeId::OKX, __VA_ARGS__)

constexpr const char* REST_ENDPOINT = "https://www.okx.com";

ApiOkx::ApiOkx(const std::vector<TradingPair> pairs, bool testMode)
        : ApiExchange(REST_ENDPOINT,
        "ws.okx.com", "8443", "/ws/v5/public",
        pairs, testMode) {
}

// [+] subscribeOrderBook

bool ApiOkx::subscribeOrderBook() {
    if (!m_connected) {
        ERROR("Not connected to Okx");
        return false;
    }

    bool success = true;

    // Split pairs into batches of MAX_SYMBOLS_PER_REQUEST
    int id = 1;
    for (auto pair : m_pairs) {
        TRACE("Subscribing to Okx order book for ", pair);
        json message;
        try {
            message["id"] = id++;
            message["op"] = "subscribe";
            message["args"] = {
                {
                    {"channel", "bbo-tbt"},
                    {"instId", tradingPairToSymbol(pair)}
                }
            };
            doWrite(message.dump());
        } catch (const std::exception& e) {
            ERROR("Error subscribing to order book batch: ", e.what(), " message: ", message.dump());
            success = false;
        }
    }

    return success;
}

bool ApiOkx::resubscribeOrderBook(const std::vector<TradingPair>& pairs) {
    if (!m_connected) {
        TRACE("Not connected to Okx");
        return false;
    }
    ERROR("Not implemented: resubscribeOrderBook");
    return false;
}


void ApiOkx::processSubscribeResponse(const json& data) {
    if(!data.contains("arg") || !data["arg"].contains("instId")) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing instId in subscribe response: ", data.dump());
        return;
    }
    auto symbol = data["arg"]["instId"];
    auto pair = symbolToTradingPair(symbol);
    if(pair == TradingPair::UNKNOWN) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_TRADING_PAIR, "Unknown trading pair: ", symbol);
        return;
    }
    auto& state = symbolStates[pair];
    state.subscribed = true;    
}

void ApiOkx::processOrderBookUpdate(const json& data) {
    ERROR("Not implemented: processOrderBookUpdate");
}

void ApiOkx::processLevel1(const json& data) {
    // example:
    // {"arg":{"channel":"bbo-tbt","instId":"BTC-USDT"},"data":[{"asks":[["105252.6","0.16271274","0","5"]],"bids":[["105252.5","1.1133491","0","19"]],"ts":"1749044565306","seqId":55729075884}]}

try {
    if(!data.contains("arg") || !data["arg"].contains("instId")) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing instId in level1 message: ", data.dump());
        return;
    }
    std::string symbol = data["arg"]["instId"];
    TradingPair pair = symbolToTradingPair(symbol);
    if(pair == TradingPair::UNKNOWN) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_TRADING_PAIR, "Unknown trading pair: ", symbol, " data: ", data.dump());
        return;
    }
    if(!data.contains("data") || !data["data"].is_array() || data["data"].size() != 1
        || !data["data"][0].contains("ts") || !data["data"][0].contains("seqId")) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing data in level1 message: ", data.dump());
        return;
    }
    int64_t seqId = data["data"][0]["seqId"];
    TRACE("Received level1 message for ", pair, " seqId: ", seqId, " data: ", data.dump());

    if(!data["data"][0].contains("asks") || !data["data"][0].contains("bids") 
        || !data["data"][0]["asks"].is_array() || !data["data"][0]["bids"].is_array()
        || data["data"][0]["asks"].size() != 1 || data["data"][0]["bids"].size() != 1 
        || data["data"][0]["asks"][0].size() != 4 || data["data"][0]["bids"][0].size() != 4
    ) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing asks or bids in level1 message: ", data.dump());
        return;
    }
    double bidPrice = std::stod(data["data"][0]["bids"][0][0].get<std::string>());
    double bidQuantity = std::stod(data["data"][0]["bids"][0][1].get<std::string>());
    double askPrice = std::stod(data["data"][0]["asks"][0][0].get<std::string>());
    double askQuantity = std::stod(data["data"][0]["asks"][0][1].get<std::string>());
    std::vector<PriceLevel> bids({{bidPrice, bidQuantity}});
    std::vector<PriceLevel> asks({{askPrice, askQuantity}});
    try {
        orderBookManager.updateOrderBookBestBidAsk(ExchangeId::OKX, pair, bidPrice, bidQuantity, askPrice, askQuantity);
    } catch (const std::exception& e) {
        ERROR("Error updating order book: ", e.what(), " data: ", data.dump());
    }
    // update symbol state
    auto& state = symbolStates[pair];
    state.lastUpdateId = seqId;
    } catch (const std::exception& e) {
        ERROR("Error processing level1 message: ", e.what(), " data: ", data.dump());
    }
}

void ApiOkx::processMessage(const json& data) {
    try {
        if (data.contains("event")) {
            if(data["event"] == "subscribe") {
                processSubscribeResponse(data);
        } else if(data["event"] == "error") {
            ERROR("Error message: ", data.dump());
        } else {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unhandled message type: ", data["event"], " data: ", data.dump());
        }
    } else if (data.contains("arg")) {
        if(data["arg"].contains("channel") && data["arg"]["channel"] == "bbo-tbt") {
            processLevel1(data);
        } else {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unhandled message type: ", data["arg"]["channel"], " data: ", data.dump());
        }
    } else {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unknown message: ", data.dump());
    }
    } catch (const std::exception& e) {
        ERROR("Error processing message: ", e.what(), " data: ", data.dump());
    }
}

void ApiOkx::processOrderBookSnapshot(const json& data, TradingPair pair) {
    ERROR("Not implemented: processOrderBookSnapshot");
}

bool ApiOkx::placeOrder(TradingPair pair, OrderType type, double price, double quantity) {
    if (!m_connected) {
        TRACE("Not connected to Okx");
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

bool ApiOkx::cancelOrder(const std::string& orderId) {
    if (!m_connected) {
        TRACE("Not connected to Okx");
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

bool ApiOkx::getBalance(const std::string& asset) {
    if (!m_connected) {
        TRACE("Not connected to Okx");
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

bool ApiOkx::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        TRACE("Not connected to Okx");
        return false;
    }
    ERROR("Not implemented: getOrderBookSnapshot");
    return false;
} 

// Implement the cooldown method for Okx-specific rate limiting
void ApiOkx::cooldown(int httpCode, const std::string& response, const std::string& endpoint) {
    bool shouldCooldown = false;
    int cooldownMinutes = 5; // Default cooldown time

    // Okx-specific rate limit handling
    if (httpCode == 429) {
        // Try to parse the retryAfter field from the response
        try {
            json errorJson = json::parse(response);
            if (errorJson.contains("retryAfter")) {
                int retryAfter = errorJson["retryAfter"].get<int>();
                TRACE("Okx rate limit retry after ", retryAfter, " seconds");
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
        TRACE("Okx entering cooldown for ", cooldownMinutes, " minutes due to HTTP ", httpCode);
        startCooldown(cooldownMinutes);
    }
}

void ApiOkx::processRateLimitHeaders(const std::string& headers) {
    // Example header: "x-mbx-used-weight: 10"
    // Parse rate limit headers from Okx
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
