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
#include "api_crypto.h"
#include "orderbook_mgr.h"
#include "tracer.h"

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;

#define TRACE(...) TRACE_THIS(TraceInstance::A_CRYPTO, ExchangeId::CRYPTO, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_CRYPTO, ExchangeId::CRYPTO, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::A_CRYPTO, ExchangeId::CRYPTO, __VA_ARGS__)

#define ERROR_CNT(_id, ...) ERROR_COUNT(TraceInstance::A_CRYPTO, _id, ExchangeId::CRYPTO, __VA_ARGS__)

constexpr const char* REST_ENDPOINT = "https://api.crypto.com/exchange/v1";

ApiCrypto::ApiCrypto(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
        const std::vector<TradingPair> pairs, bool testMode)
        : ApiExchange(orderBookManager, timersMgr,  
        REST_ENDPOINT,
        "stream.crypto.com", "443", "/exchange/v1/market",
        pairs, testMode) {
}

// [+] subscribeOrderBook

bool ApiCrypto::subscribeOrderBook() {
    if (!m_connected) {
        ERROR("Not connected to Crypto");
        return false;
    }
    int id = 1;
    std::vector<std::string> symbols;
    for (auto pair : m_pairs) {
        symbols.push_back("ticker." + tradingPairToSymbol(pair) + "-PERP");
        // TRACE("Subscribing to Crypto order book for ");
        auto symbol = "ticker." + tradingPairToSymbol(pair) + "-PERP";
        json message;
        try {
            message["id"] = id++;
            message["method"] = "subscribe";
            message["params"] = {{"channels", symbol}};    
            doWrite(message.dump());
        } catch (const std::exception& e) {
            ERROR("Error subscribing to order book batch: ", e.what(), " message: ", message.dump());
            return false;
        }

    }

    for (auto pair : m_pairs) {
        auto& state = symbolStates[pair];
        state.subscribed = true;    
    }
    return true;
}

bool ApiCrypto::resubscribeOrderBook(const std::vector<TradingPair>& pairs) {
    if (!m_connected) {
        TRACE("Not connected to Crypto");
        return false;
    }
    ERROR("Not implemented: resubscribeOrderBook");
    return false;
}

// [+] processMessage

void ApiCrypto::processMessage(const json& data) {
    TRACE("Processing message: ", data.dump());
    try {
        if(data.contains("code") && data["code"] != 0) {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Error message: ", data.dump());
            return;
        }
        if (data.contains("method")) {
            if(data["method"] == "subscribe") {
                processLevel1(data); // updates are sent with subscribe tag
            } else if(data["method"] == "error") {
                ERROR_CNT(CountableTrace::A_REJECTED_ORDER, "Error message, code: ", data["code"], " data: ", data.dump());
            } else if (data["method"] == "public/heartbeat") {
                if (!data.contains("id")) {
                    ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing id in heartbeat message: ", data.dump());
                    return;
                }
                json message;
                message["id"] = data["id"];
                message["method"] = "public/respond-heartbeat";
                doWrite(message.dump());
            } else {
                ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unhandled message type: ", data["event"], " data: ", data.dump());
            }
        } else {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Unknown message: ", data.dump());
        }   
    } catch (const std::exception& e) {
        ERROR("Error processing message: ", e.what(), " data: ", data.dump());
    }
}

void ApiCrypto::processOrderBookUpdate(const json& data) {
    ERROR("Not implemented: processOrderBookUpdate");
}

void ApiCrypto::processLevel1(const json& data) {
    // example:
    // {"id":1,"method":"subscribe","code":0,"result":{"instrument_name":"BTCUSD-PERP","subscription":"ticker.BTCUSD-PERP","channel":"ticker","data":[{"h":"106621.9","l":"104206.1","a":"105390.6","c":"-0.0108","b":"105395.1","bs":"0.1865","k":"105395.2","ks":"0.2914","i":"BTCUSD-PERP","v":"8195.0690","vv":"864533755.30","oi":"6121.7822","t":1749053480565}]}}

    try {
        if (data.contains("code") && data["code"] != 0) {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Error in subscribe message: ", data.dump());
            return;
        }

        if (!data.contains("result")) {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing result in ticker message: ", data.dump());
            return;
        }

        if (!data["result"].contains("data") || !data["result"]["data"].is_array() || data["result"]["data"].empty()) {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Invalid ticker message format: ", data.dump());
            return;
        }
        if(!data["result"]["data"][0].contains("i") || !data["result"]["data"][0]["i"].is_string() || data["result"]["data"][0]["i"].empty()) {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing instrument name in ticker message: ", data.dump());
            return;
        }

        const auto& tickerData = data["result"]["data"][0];
        const auto& instrumentName = tickerData["i"].get<std::string>();
        
        // Extract symbol from the data
        int len = instrumentName.length();
        if (instrumentName.substr(len-5) != "-PERP") {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Invalid instrument name in ticker message: ", data.dump());
            return;
        }
        // skip last 5 symbols (-PERP)
        std::string symbol = instrumentName.substr(0, len - 5);
        TradingPair pair = symbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            ERROR_CNT(CountableTrace::A_UNKNOWN_TRADING_PAIR, "Unknown trading pair: ", symbol, " data: ", data.dump());
            return;
        }

        // Extract best bid/ask prices and quantities
        if (!tickerData.contains("b") || !tickerData.contains("bs") || !tickerData.contains("k") || !tickerData.contains("ks")) {
            ERROR_CNT(CountableTrace::A_UNKNOWN_MESSAGE_RECEIVED, "Missing price or quantity in ticker message: ", data.dump());
            return;
        }

        // Handle null values for prices and quantities
        double bidPrice = tickerData["b"].is_null() ? 0 : std::stod(tickerData["b"].get<std::string>());
        double bidQuantity = tickerData["bs"].is_null() ? 0 : std::stod(tickerData["bs"].get<std::string>());
        double askPrice = tickerData["k"].is_null() ? 0 : std::stod(tickerData["k"].get<std::string>());
        double askQuantity = tickerData["ks"].is_null() ? 0 : std::stod(tickerData["ks"].get<std::string>());

        // Update order book with best prices
        try {
            m_orderBookManager.updateOrderBookBestBidAsk(ExchangeId::CRYPTO, pair, bidPrice, bidQuantity, askPrice, askQuantity);
            TRACE("Updated best prices for ", symbol, 
                  " bid=", bidPrice, "(", bidQuantity, ")", 
                  " ask=", askPrice, "(", askQuantity, ")");
        } catch (const std::exception& e) {
            ERROR("Error updating order book: ", e.what(), " data: ", data.dump());
        }

        // Update symbol state with timestamp if available
        auto& state = symbolStates[pair];
        if (tickerData.contains("t")) {
            state.lastUpdateId = tickerData["t"].get<int64_t>();
        }
    } catch (const std::exception& e) {
        ERROR("Error processing level1 message: ", e.what(), " data: ", data.dump());
    }
}

void ApiCrypto::processOrderBookSnapshot(const json& data, TradingPair pair) {
    ERROR("Not implemented: processOrderBookSnapshot");
}

bool ApiCrypto::placeOrder(TradingPair pair, OrderType type, double price, double quantity) {
    if (!m_connected) {
        TRACE("Not connected to Crypto");
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

bool ApiCrypto::cancelOrder(const std::string& orderId) {
    if (!m_connected) {
        TRACE("Not connected to Crypto");
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

bool ApiCrypto::getBalance(const std::string& asset) {
    if (!m_connected) {
        TRACE("Not connected to Crypto");
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

bool ApiCrypto::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        TRACE("Not connected to Crypto");
        return false;
    }
    ERROR("Not implemented: getOrderBookSnapshot");
    return false;
} 

// Implement the cooldown method for Crypto-specific rate limiting
void ApiCrypto::cooldown(int httpCode, const std::string& response, const std::string& endpoint) {
    bool shouldCooldown = false;
    int cooldownMinutes = 5; // Default cooldown time

    // Crypto-specific rate limit handling
    if (httpCode == 429) {
        // Try to parse the retryAfter field from the response
        try {
            json errorJson = json::parse(response);
            if (errorJson.contains("retryAfter")) {
                int retryAfter = errorJson["retryAfter"].get<int>();
                TRACE("Crypto rate limit retry after ", retryAfter, " seconds");
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
        TRACE("Crypto entering cooldown for ", cooldownMinutes, " minutes due to HTTP ", httpCode);
        startCooldown(cooldownMinutes);
    }
}

void ApiCrypto::processRateLimitHeaders(const std::string& headers) {
    // Example header: "x-mbx-used-weight: 10"
    // Parse rate limit headers from Crypto
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
