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
#include "api_bybit.h"
#include "orderbook_mgr.h"
#include "tracer.h"

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;

#define TRACE(...) TRACE_THIS(TraceInstance::A_BYBIT, ExchangeId::BYBIT, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_BYBIT, ExchangeId::BYBIT, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::A_BYBIT, ExchangeId::BYBIT, __VA_ARGS__)

#define ERROR_CNT(_id, ...) ERROR_COUNT(TraceInstance::A_BYBIT, _id, ExchangeId::BYBIT, __VA_ARGS__)

constexpr const char* REST_ENDPOINT = "https://stream.bybit.com";

ApiBybit::ApiBybit(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
        const std::vector<TradingPair> pairs, bool testMode)
        : ApiExchange(orderBookManager, timersMgr,  
        REST_ENDPOINT,
        "stream.bybit.com", "443", "/v5/public/spot",
        pairs, testMode) {
}

void ApiBybit::processSubscribeResponse(const json& data) {
    if(!data.contains("success") || data["success"] == false) {
        ERROR("Subscription failed: ", data.dump());
        return;
    }
    // set the subscription state for all symbols
    for (const auto& pair : m_pairs) {
        auto& state = symbolStates[pair];
        state.subscribed = true;    
    }
}

void ApiBybit::processOrderBookUpdate(const json& data) {
    ERROR("Not implemented: processOrderBookUpdate");
}

void ApiBybit::processLevel1(const json& data) {
    // example:
    // {"topic":"orderbook.1.BTCUSDT","ts":1748980103101,"type":"snapshot","data":{"s":"BTCUSDT","b":[["106285.8","0.794865"]],"a":[["106285.9","1.269905"]],"u":943951,"seq":75829523783},"cts":1748980103098}

    if(!data.contains("topic")) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing topic in level1 message: ", data.dump());
        return;
    }
    std::string topic = data["topic"];
    // example: "orderbook.1.BTCUSDT"
    if(topic.find("orderbook.1.") != 0) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Invalid topic in level1 message: ", topic, " data: ", data.dump());
        return;
    }
    if(!data.contains("data") || !data["data"].contains("s") || !data["data"].contains("u")) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing data in level1 message: ", data.dump());
        return;
    }
    std::string symbol = data["data"]["s"];
    TradingPair pair = symbolToTradingPair(symbol);
    TRACE("Received level1 message for ", pair, " data: ", data.dump());

    if(pair == TradingPair::UNKNOWN) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_TRADING_PAIR, "Unknown trading pair: ", symbol, " data: ", data.dump());
        return;
    }
    int64_t timestamp = data["data"]["u"];

    if(!data["data"].contains("a") || !data["data"].contains("b") 
        || !data["data"]["a"].is_array() || !data["data"]["b"].is_array()
        || data["data"]["a"].size() != 1 || data["data"]["b"].size() != 1 
        || data["data"]["a"][0].size() != 2 || data["data"]["b"][0].size() != 2
    ) {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing asks or bids in level1 message: ", data.dump());
        return;
    }
    double bidPrice = std::stod(data["data"]["b"][0][0].get<std::string>());
    double bidQuantity = std::stod(data["data"]["b"][0][1].get<std::string>());
    double askPrice = std::stod(data["data"]["a"][0][0].get<std::string>());
    double askQuantity = std::stod(data["data"]["a"][0][1].get<std::string>());
    std::vector<PriceLevel> bids({{bidPrice, bidQuantity}});
    std::vector<PriceLevel> asks({{askPrice, askQuantity}});
    try {
        m_orderBookManager.updateOrderBookBestBidAsk(ExchangeId::BYBIT, pair, bidPrice, bidQuantity, askPrice, askQuantity);
    } catch (const std::exception& e) {
        ERROR("Error updating order book: ", e.what(), " data: ", data.dump());
    }
    // update symbol state
    auto& state = symbolStates[pair];
    state.lastUpdateId = timestamp;
}

void ApiBybit::processMessage(const json& data) {
    if (data.contains("ret_msg")) {
        if(data["ret_msg"] == "subscribe") {
            processSubscribeResponse(data);
        } else {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unhandled message type: ", data["type"], " data: ", data.dump());
        }
    } else if (data.contains("type")) {
        if(data["type"] == "snapshot") {
            processLevel1(data);
        } else if (data["type"] == "update") {
            ERROR("Not implemented: processTickerUpdate: ", data.dump());
        } else if (data["type"] == "error") {
            ERROR("Error message: ", data.dump());
        } else {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unhandled message type: ", data["type"], " data: ", data.dump());
        }
    } else {
        ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unknown message: ", data.dump());
    }
}

void ApiBybit::processOrderBookSnapshot(const json& data, TradingPair pair) {
    ERROR("Not implemented: processOrderBookSnapshot");
}

bool ApiBybit::placeOrder(TradingPair pair, OrderType type, double price, double quantity) {
    if (!m_connected) {
        TRACE("Not connected to Bybit");
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

bool ApiBybit::cancelOrder(const std::string& orderId) {
    if (!m_connected) {
        TRACE("Not connected to Bybit");
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

bool ApiBybit::getBalance(const std::string& asset) {
    if (!m_connected) {
        TRACE("Not connected to Bybit");
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

bool ApiBybit::subscribeOrderBook() {
    if (!m_connected) {
        ERROR("Not connected to Bybit");
        return false;
    }

    const size_t MAX_SYMBOLS_PER_REQUEST = 10;
    bool success = true;

    // Split pairs into batches of MAX_SYMBOLS_PER_REQUEST
    for (size_t i = 0; i < m_pairs.size(); i += MAX_SYMBOLS_PER_REQUEST) {
        std::vector<std::string> symbolsVec;
        std::string symbols;
        
        // Get the current batch of pairs
        size_t end = std::min(i + MAX_SYMBOLS_PER_REQUEST, m_pairs.size());
        for (size_t j = i; j < end; j++) {
            const auto& pair = m_pairs[j];
            symbolsVec.push_back("orderbook.1." + tradingPairToSymbol(pair));
            symbols += tradingPairToSymbol(pair) + ",";
        }
        if (!symbols.empty()) {
            symbols.pop_back(); // Remove the last comma
        }

        TRACE("Subscribing to Bybit order book batch ", (i/MAX_SYMBOLS_PER_REQUEST + 1), 
              " for pairs: ", symbols);

        json message;
        try {
            message["op"] = "subscribe";
            message["args"] = symbolsVec;

            TRACE("Subscribing to Bybit order book with message: ", message.dump());
            doWrite(message.dump());
        } catch (const std::exception& e) {
            ERROR("Error subscribing to order book batch: ", e.what(), " message: ", message.dump());
            success = false;
        }
    }

    return success;
}

bool ApiBybit::resubscribeOrderBook(const std::vector<TradingPair>& pairs) {
    if (!m_connected) {
        TRACE("Not connected to Bybit");
        return false;
    }
    ERROR("Not implemented: resubscribeOrderBook");
    return false;
}

bool ApiBybit::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        TRACE("Not connected to Bybit");
        return false;
    }
    ERROR("Not implemented: getOrderBookSnapshot");
    return false;
} 

// Implement the cooldown method for Bybit-specific rate limiting
void ApiBybit::cooldown(int httpCode, const std::string& response, const std::string& endpoint) {
    bool shouldCooldown = false;
    int cooldownMinutes = 5; // Default cooldown time

    // Bybit-specific rate limit handling
    if (httpCode == 429) {
        // Try to parse the retryAfter field from the response
        try {
            json errorJson = json::parse(response);
            if (errorJson.contains("retryAfter")) {
                int retryAfter = errorJson["retryAfter"].get<int>();
                TRACE("Bybit rate limit retry after ", retryAfter, " seconds");
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
        TRACE("Bybit entering cooldown for ", cooldownMinutes, " minutes due to HTTP ", httpCode);
        startCooldown(cooldownMinutes);
    }
}

void ApiBybit::processRateLimitHeaders(const std::string& headers) {
    // Example header: "x-mbx-used-weight: 10"
    // Parse rate limit headers from Bybit
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
