#include "api_kraken.h"
#include "orderbook.h"
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
#include "types.h"

using json = nlohmann::json;
using tcp = boost::asio::ip::tcp;

constexpr const char* REST_ENDPOINT = "https://api.kraken.com/0/public";

#define TRACE(...) TRACE_THIS(TraceInstance::A_KRAKEN, ExchangeId::KRAKEN, __VA_ARGS__)

ApiKraken::ApiKraken(OrderBookManager& orderBookManager, TimersMgr& timersMgr, bool testMode)
    : ApiExchange(orderBookManager, timersMgr, "ws.kraken.com", "443", 
    REST_ENDPOINT, "/ws", testMode) {
    // Initialize symbol map
    m_symbolMap[TradingPair::BTC_USDT] = "XBT/USD";
    m_symbolMap[TradingPair::ETH_USDT] = "ETH/USD";
    m_symbolMap[TradingPair::XTZ_USDT] = "XTZ/USD";
}

// Kraken-specific symbol mapping
TradingPair ApiKraken::symbolToTradingPair(const std::string& symbol) const {
    std::string lowerSymbol = toLower(symbol);
    
    if (lowerSymbol == "xbt/usd" || lowerSymbol == "btc/usd" || lowerSymbol == "xxbtzusd") return TradingPair::BTC_USDT;
    if (lowerSymbol == "eth/usd" || lowerSymbol == "xethzusd") return TradingPair::ETH_USDT;
    if (lowerSymbol == "xtz/usd" || lowerSymbol == "xtzusdt") return TradingPair::XTZ_USDT;
    
    return TradingPair::UNKNOWN;
}

std::string ApiKraken::tradingPairToSymbol(TradingPair pair) const {
    auto it = m_symbolMap.find(pair);
    if (it != m_symbolMap.end()) {
        return it->second;
    }
    throw std::runtime_error("Unsupported trading pair");
}

bool ApiKraken::subscribeOrderBook(std::vector<TradingPair> pairs) {
    if (!m_connected) {
        TRACE("Not connected to Kraken");
        return false;
    }

    std::vector<std::string> symbols;
    for (const auto& pair : pairs) {
        symbols.push_back(tradingPairToSymbol(pair));
    }

    try {
        // kraken doesn't support subscription for best bid/ask only
        // so we need to subscribe to the order book
        json subscription = {
            {"event", "subscribe"},
            {"subscription", {
                {"name", "book"},
                {"depth", 10} // min depth is 10
            }},
            {"pair", symbols}
        };

        TRACE("Subscribing to Kraken order book: ", subscription.dump());
        doWrite(subscription.dump());
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

bool ApiKraken::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        TRACE("Not connected to Kraken");
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);
        // Remove the forward slash for the API request
        symbol.erase(std::remove(symbol.begin(), symbol.end(), '/'), symbol.end());
        std::string endpoint = "/Depth";
        std::string params = "pair=" + symbol + "&count=1000";

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

void ApiKraken::processMessage(const std::string& message) {
    try {
        json data = json::parse(message);
        
        // Truncated to 500 characters  
        std::string logMessage = message;
        if (logMessage.length() > 500) {
            logMessage = logMessage.substr(0, 497) + "...";
        }
        TRACE("Processing message: ", logMessage);
        
        // examples:
        // {"method":"subscribe","result":{"channel":"book","depth":10,"snapshot":true,"symbol":"ETH/USD"},"success":true,"time_in":"2025-05-06T09:17:26.058856Z","time_out":"2025-05-06T09:17:26.058894Z"}
        // {"channel":"book","type":"snapshot","data":[{"symbol":"ETH/USD","bids":[{"price":1797.24,"qty":0.43393711},{"price":1797.18,"qty":139.10616255},{"price":1797.13,"qty":139.10995927},{"price":1797.12,"qty":0.80386470},{"price":1797.07,"qty":139.11517673},{"price":1796.98,"qty":5.22282896},{"price":1796.97,"qty":12.17138476},{"price":1796.95,"qty":139.69390939},{"price":1796.88,"qty":4.21870000},{"price":1796.85,"qty":1.22932317}],"asks":[{"price":1797.25,"qty":1.40355852},{"price":1797.28,"qty":10.82400000},{"price":1797.29,"qty":139.09899871},{"price":1797.30,"qty":139.09783403},{"price":1797.37,"qty":139.09216827},{"price":1797.41,"qty":5.24638952},{"price":1797.42,"qty":0.01780330},{"price":1797.46,"qty":33.05700000},{"price":1797.48,"qty":3.11556555},{"price":1797.49,"qty":139.49306646}],"checksum":2606672715}]}
        // {"channel":"book","type":"update","data":[{"symbol":"ETH/USD","bids":[],"asks":[{"price":1797.29,"qty":0.00000000},{"price":1797.52,"qty":1.56601318}],"checksum":3329440863,"timestamp":"2025-05-06T09:17:26.208075Z"}]}


        // Check if it's a subscription response
        if (data.is_object() && data.contains("event")) {
            if (data["event"] == "subscriptionStatus") {
                TRACE("Subscription status: ", data["status"]);
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

                auto& state = symbolStates[tradingPair];
                if (!state.hasSnapshot) {
                    TRACE("Skipping update for ", pair, " - no snapshot yet");
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
                            } else {
                                // If quantity is 0, it's a delete - we'll pass it through with 0 quantity
                                asks.push_back({price, 0});
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
                            } else {
                                // If quantity is 0, it's a delete - we'll pass it through with 0 quantity
                                bids.push_back({price, 0});
                            }
                        }
                    }
                }

                if (!bids.empty() || !asks.empty()) {
                    TRACE("Updating order book for ", pair, " with ", bids.size(), " bids and ", asks.size(), " asks");
                    m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, tradingPair, bids, asks);
                }
            }
        }
    } catch (const std::exception& e) {
        TRACE("Error processing message: ", e.what());
    }
}

void ApiKraken::processOrderBookUpdate(const json& data) {
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

        TRACE("Updating order book for ", symbol, " with ", bids.size(), " bids and ", asks.size(), " asks");
        m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, bids, asks);
    } catch (const std::exception& e) {
        TRACE("Error processing order book update: ", e.what());
    }
}

void ApiKraken::processOrderBookSnapshot(const json& data, TradingPair pair) {
    try {
        auto& state = symbolStates[pair];
        state.hasSnapshot = true;

        TRACE("Processing order book snapshot for ", tradingPairToSymbol(pair));
        
        // Check if we have a valid response with result data
        if (!data.contains("result"))
            throw std::runtime_error("Invalid response format: missing result data for " + tradingPairToSymbol(pair));

        for (const auto& [symbol, orderBook] : data["result"].items()) {
            TradingPair receivedPair = symbolToTradingPair(symbol);
            if (receivedPair != pair) {
                throw std::runtime_error("Invalid response format: unknown trading symbol " + symbol);
            }
        
            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;
            
            // Process bids
            if (orderBook.contains("bids")) {
                for (const auto& bid : orderBook["bids"]) {
                    if (bid.size() >= 2) {
                        double price = std::stod(bid[0].get<std::string>());
                        double quantity = std::stod(bid[1].get<std::string>());
                        if (quantity > 0) {
                            bids.push_back({price, quantity});
                        }
                    }
                }
            }
            
            // Process asks
            if (orderBook.contains("asks")) {
                for (const auto& ask : orderBook["asks"]) {
                    if (ask.size() >= 2) {
                        double price = std::stod(ask[0].get<std::string>());
                        double quantity = std::stod(ask[1].get<std::string>());
                        if (quantity > 0) {
                            asks.push_back({price, quantity});
                        }
                    }
                }
            }

            // Update the order book with all bids and asks at once
            if (!bids.empty() || !asks.empty()) {
                TRACE("Updating order book for ", symbol, " with ", bids.size(), " bids and ", asks.size(), " asks");
                m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, bids, asks);
            }
            
            TRACE("Processed order book snapshot for ", tradingPairToSymbol(pair));
            if (m_snapshotCallback) {
                m_snapshotCallback(true);
            }
        }
    } catch (const std::exception& e) {
        TRACE("Error processing order book snapshot: ", e.what());
        if (m_snapshotCallback) {
            m_snapshotCallback(false);
        }
    }
}

bool ApiKraken::placeOrder(TradingPair pair, OrderType type, double price, double quantity) {
    if (!m_connected) {
        TRACE("Not connected to Kraken");
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);
        std::string side = (type == OrderType::BUY) ? "buy" : "sell";
        
        std::stringstream ss;
        ss << "pair=" << symbol
           << "&type=" << side
           << "&ordertype=limit"
           << "&volume=" << std::fixed << std::setprecision(8) << quantity
           << "&price=" << std::fixed << std::setprecision(8) << price;

        json response = makeHttpRequest("/AddOrder", ss.str(), "POST");
        TRACE("Order placed successfully: ", response.dump());
        return true;
    } catch (const std::exception& e) {
        TRACE("Error placing order: ", e.what());
        return false;
    }
}

bool ApiKraken::cancelOrder(const std::string& orderId) {
    if (!m_connected) {
        TRACE("Not connected to Kraken");
        return false;
    }

    try {
        std::string params = "txid=" + orderId;
        json response = makeHttpRequest("/CancelOrder", params, "POST");
        TRACE("Order cancelled successfully: ", response.dump());
        return true;
    } catch (const std::exception& e) {
        TRACE("Error cancelling order: ", e.what());
        return false;
    }
}

bool ApiKraken::getBalance(const std::string& asset) {
    if (!m_connected) {
        TRACE("Not connected to Kraken");
        return false;
    }

    try {
        json response = makeHttpRequest("/Balance", "", "POST");
        
        if (response.contains("result") && response["result"].contains(asset)) {
            TRACE("Balance for ", asset, ": ", response["result"][asset].get<std::string>());
            return true;
        }
        
        TRACE("No balance found for asset: ", asset);
        return false;
    } catch (const std::exception& e) {
        TRACE("Error getting balance: ", e.what());
        return false;
    }
}

// Implement the cooldown method for Kraken-specific rate limiting
void ApiKraken::cooldown(int httpCode, const std::string& response, const std::string& endpoint) {
    // Only call base class cooldown if there's an actual error
    if (httpCode > 0) {
        ApiExchange::cooldown(httpCode, response, endpoint);
    }
}

void ApiKraken::processRateLimitHeaders(const std::string& headers) {
    // Example header: "CF-RateLimit-Remaining: 50"
    // Parse rate limit headers from Kraken
    size_t pos = headers.find("CF-RateLimit-Remaining:");
    if (pos != std::string::npos) {
        try {
            std::string value = headers.substr(pos + 22); // Skip "CF-RateLimit-Remaining:"
            int remaining = std::stoi(value);
            // Update rate limit info
            updateRateLimit("api", 60, remaining, 60);
        } catch (const std::exception& e) {
            TRACE("Failed to parse rate limit header: ", e.what());
        }
    }
}
