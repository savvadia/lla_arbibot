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

#define TRACE(...) TRACE_THIS(TraceInstance::A_BINANCE, ExchangeId::BINANCE, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_BINANCE, ExchangeId::BINANCE, __VA_ARGS__)
#define ERROR(...) ERROR_BASE(TraceInstance::A_BINANCE, ExchangeId::BINANCE, __VA_ARGS__)

constexpr const char* REST_ENDPOINT = "https://api.binance.com/api/v3";

ApiBinance::ApiBinance(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
        const std::vector<TradingPair> pairs, bool testMode)
        : ApiExchange(orderBookManager, timersMgr,  
        "stream.binance.com", "9443", REST_ENDPOINT, "/ws/stream",
        pairs, testMode) {
    // Initialize symbol map
    m_symbolMap[TradingPair::ADA_USDT] = "ADAUSDT";
    m_symbolMap[TradingPair::ATOM_USDT] = "ATOMUSDT";
    m_symbolMap[TradingPair::BCH_USDT] = "BCHUSDT";
    m_symbolMap[TradingPair::BTC_USDT] = "BTCUSDT";
    m_symbolMap[TradingPair::DOGE_USDT] = "DOGEUSDT";
    m_symbolMap[TradingPair::DOT_USDT] = "DOTUSDT";
    m_symbolMap[TradingPair::EOS_USDT] = "EOSUSDT";
    m_symbolMap[TradingPair::ETH_USDT] = "ETHUSDT";
    m_symbolMap[TradingPair::LINK_USDT] = "LINKUSDT";
    m_symbolMap[TradingPair::SOL_USDT] = "SOLUSDT";
    m_symbolMap[TradingPair::XRP_USDT] = "XRPUSDT";
    m_symbolMap[TradingPair::XTZ_USDT] = "XTZUSDT";
}

// Binance-specific symbol mapping
TradingPair ApiBinance::symbolToTradingPair(const std::string& symbol) const {
    std::string lowerSymbol = symbol;
    std::transform(lowerSymbol.begin(), lowerSymbol.end(), lowerSymbol.begin(), ::tolower);

    if (lowerSymbol == "adausdt") return TradingPair::ADA_USDT;
    if (lowerSymbol == "atomusdt") return TradingPair::ATOM_USDT;
    if (lowerSymbol == "bchusdt") return TradingPair::BCH_USDT;
    if (lowerSymbol == "btcusdt") return TradingPair::BTC_USDT;
    if (lowerSymbol == "dogeusdt") return TradingPair::DOGE_USDT;
    if (lowerSymbol == "dotusdt") return TradingPair::DOT_USDT;
    if (lowerSymbol == "eosusdt") return TradingPair::EOS_USDT;
    if (lowerSymbol == "ethusdt") return TradingPair::ETH_USDT;
    if (lowerSymbol == "linkusdt") return TradingPair::LINK_USDT;
    if (lowerSymbol == "solusdt") return TradingPair::SOL_USDT;
    if (lowerSymbol == "xrpusdt") return TradingPair::XRP_USDT;
    if (lowerSymbol == "xtzusdt") return TradingPair::XTZ_USDT;
    return TradingPair::UNKNOWN;
}

std::string ApiBinance::tradingPairToSymbol(TradingPair pair) const {
    auto it = m_symbolMap.find(pair);
    if (it != m_symbolMap.end()) {
        return it->second;
    }
    ERROR("Unsupported trading pair: ", pair);
    throw std::runtime_error("Unsupported trading pair");
}

void ApiBinance::processMessage(const std::string& message) {
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
            ERROR("Unhandled message type: ", message);
        }
    } catch (const std::exception& e) {
        ERROR("Error processing message: ", e.what());
    }
}

void ApiBinance::processBookTicker(const json& data) {
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
        m_orderBookManager.updateOrderBookBestBidAsk(ExchangeId::BINANCE, pair, bidPrice, bidQuantity, askPrice, askQuantity);
    
    } catch (const std::exception& e) {
        ERROR("Error processing bookTicker: ", data.dump().substr(0, 300), " ", e.what());
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
            m_orderBookManager.updateOrderBook(ExchangeId::BINANCE, pair, bids, asks);
        }

        // Mark that we have a snapshot for this symbol
        setSymbolSnapshotState(pair, true);
        TRACE("Got order book snapshot for ", symbol);
    } catch (const std::exception& e) {
        ERROR("Error processing order book update: ", e.what());
    }
}

void ApiBinance::processOrderBookSnapshot(const json& data, TradingPair pair) {
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
            m_orderBookManager.updateOrderBook(ExchangeId::BINANCE, pair, bids, asks);
            
            TRACE("Processed order book snapshot for ", tradingPairToSymbol(pair),
                " last update: ", m_orderBookManager.getOrderBook(ExchangeId::BINANCE, pair).getLastUpdate());
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
        ERROR("Error placing order: ", e.what());
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
        ERROR("Error cancelling order: ", e.what());
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
        ERROR("Error getting balance: ", e.what());
        return false;
    }
}

bool ApiBinance::subscribeOrderBook() {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }

    TRACE("Subscribing to Binance order book for ", m_pairs.size(), " pairs");

    try {
        json message;
        message["id"] = 1;
        message["method"] = "SUBSCRIBE";
        message["params"] = json::array();
        for (const auto& pair : m_pairs) {
            std::string symbol = toLower(tradingPairToSymbol(pair));
            message["params"].emplace_back(symbol + "@bookTicker");
        }

        TRACE("Subscribing to Binance order book with message: ", message.dump());
        doWrite(message.dump());  

        // Store the subscription state
        for (const auto& pair : m_pairs) {
            auto& state = symbolStates[pair];
            std::string symbol = tradingPairToSymbol(pair);    
            state.subscribed = true;
            setSymbolSnapshotState(pair, false);
            TRACE("Subscription state for ", symbol, ": subscribed=", state.subscribed, " hasSnapshot=", state.hasSnapshot());
        }
        
        return true;
    } catch (const std::exception& e) {
        ERROR("Error subscribing to order book: ", e.what());
        return false;
    }
}

bool ApiBinance::resubscribeOrderBook(const std::vector<TradingPair>& pairs) {
    if (!m_connected) {
        TRACE("Not connected to Binance");
        return false;
    }
    ERROR("Not implemented: resubscribeOrderBook");
    return false;
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
        ERROR("Error getting order book snapshot: ", e.what());
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
