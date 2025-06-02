#include "api_kraken.h"
#include "orderbook_mgr.h"
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
#include <zlib.h>  // for crc32
#include "tracer.h"
#include "types.h"

using tcp = boost::asio::ip::tcp;

constexpr const char* REST_ENDPOINT = "https://api.kraken.com/0/public";

#define TRACE(...) TRACE_THIS(TraceInstance::A_KRAKEN, ExchangeId::KRAKEN, __VA_ARGS__)
#define DEBUG(...) DEBUG_THIS(TraceInstance::A_KRAKEN, ExchangeId::KRAKEN, __VA_ARGS__)
#define ERROR(...) ERROR_THIS(TraceInstance::A_KRAKEN, ExchangeId::KRAKEN, __VA_ARGS__)
#define ERROR_CNT(_id, ...) ERROR_COUNT(TraceInstance::A_KRAKEN, _id, ExchangeId::KRAKEN, __VA_ARGS__)
#define TRACE_CNT(_id, ...) TRACE_COUNT(TraceInstance::A_KRAKEN, _id, ExchangeId::KRAKEN, __VA_ARGS__)

ApiKraken::ApiKraken(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
    const std::vector<TradingPair> pairs, bool testMode)
    : ApiExchange(orderBookManager, timersMgr, 
        REST_ENDPOINT,
        "ws.kraken.com", "443", "/ws/v2",
        pairs, testMode) {
}

bool ApiKraken::handleSubscribeUnsubscribe(const std::vector<TradingPair>& pairs, bool subscribe) {
    if (pairs.empty()) {
        ERROR("No pairs to subscribe/unsubscribe");
        return false;
    }
    std::vector<std::string> symbols;
    std::string symbolsStr;
    for (const auto& pair : pairs) {
        symbols.push_back(tradingPairToSymbol(pair));
        symbolsStr += tradingPairToSymbol(pair) + ", ";

        symbolStates[pair].subscribed = subscribe;
    }

    TRACE(subscribe ? "Subscribing to " : "Unsubscribing from ", symbolsStr);

    json request = {
        {"method", subscribe ? "subscribe" : "unsubscribe"},
        {"params", {
            {"channel", "book"},
            {"symbol", symbols}
        }}
    };

    try  {
        doWrite(request.dump());
        return true;
    } catch (const std::exception& e) {
        ERROR("Error ", subscribe ? "subscribing to" : "unsubscribing from", " order book: ", e.what());
        return false;
    }
}
bool ApiKraken::subscribeOrderBook() {
    return handleSubscribeUnsubscribe(m_pairs, true);
}

bool ApiKraken::resubscribeOrderBook(const std::vector<TradingPair>& pairs) {
    if (!m_connected) {
        ERROR("Not connected to Kraken");
        return false;
    }
    std::vector<TradingPair> pairsToUnsubscribe;
    for (const auto& pair : pairs) {
        // if subscribed, unsubscribe
        if (symbolStates[pair].subscribed) {
            pairsToUnsubscribe.push_back(pair);
        }
    }

    // unsubscribe first
    if (pairsToUnsubscribe.size() > 0 && !handleSubscribeUnsubscribe(pairsToUnsubscribe, false)) {
        ERROR("Failed to unsubscribe from Kraken order book");
        return false;
    }
    // subscribe again
    if (!handleSubscribeUnsubscribe(pairs, true)) {
        ERROR("Failed to subscribe to Kraken order book");
        return false;
    }
    return true;
}

bool ApiKraken::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        ERROR("Not connected to Kraken");
        return false;
    }
    return true;

    // snapshot will come from book subscription as the first msg
}

void ApiKraken::processMessage(const std::string& message) {
    try {
        json data = json::parse(message);
        DEBUG("Processing message: ", data.dump());
        
        // examples:
        // {"method":"subscribe","result":{"channel":"book","depth":10,"snapshot":true,"symbol":"ETH/USD"},"success":true,"time_in":"2025-05-06T09:17:26.058856Z","time_out":"2025-05-06T09:17:26.058894Z"}
        // {"channel":"book","type":"snapshot","data":[{"symbol":"ETH/USD","bids":[{"price":1797.24,"qty":0.43393711},{"price":1797.18,"qty":139.10616255},{"price":1797.13,"qty":139.10995927},{"price":1797.12,"qty":0.80386470},{"price":1797.07,"qty":139.11517673},{"price":1796.98,"qty":5.22282896},{"price":1796.97,"qty":12.17138476},{"price":1796.95,"qty":139.69390939},{"price":1796.88,"qty":4.21870000},{"price":1796.85,"qty":1.22932317}],"asks":[{"price":1797.25,"qty":1.40355852},{"price":1797.28,"qty":10.82400000},{"price":1797.29,"qty":139.09899871},{"price":1797.30,"qty":139.09783403},{"price":1797.37,"qty":139.09216827},{"price":1797.41,"qty":5.24638952},{"price":1797.42,"qty":0.01780330},{"price":1797.46,"qty":33.05700000},{"price":1797.48,"qty":3.11556555},{"price":1797.49,"qty":139.49306646}],"checksum":2606672715}]}
        // {"channel":"book","type":"update","data":[{"symbol":"ETH/USD","bids":[],"asks":[{"price":1797.29,"qty":0.00000000},{"price":1797.52,"qty":1.56601318}],"checksum":3329440863,"timestamp":"2025-05-06T09:17:26.208075Z"}]}

        if (!data.is_object()) {
            ERROR("Invalid message format: ", data.dump());
            return;
        }

        if (data.contains("method")) {
            if (data["method"] == "subscribe") {
                if (data["success"] == true) {
                    TRACE("Subscription successful: ", data.dump());
                } else {
                    ERROR("Subscription failed: ", data.dump());
                }
            }
            return;
        }

        if (data.contains("channel")) {
            if (data["channel"] == "status") {
                TRACE("Got connection status: ", data.dump());
            } else if (data["channel"] == "heartbeat") {
                DEBUG("Got heartbeat: ", data.dump());
            } else if (data["channel"] == "book") {
                processOrderBookUpdate(data);
            } else {
                ERROR("Unknown channel: ", data.dump());
            }
        }
    } catch (const std::exception& e) {
        ERROR("Error processing message: ", e.what());
    }
}

void ApiKraken::processOrderBookUpdate(const json& data) {
    static int countCalls = 0;
    if (!data.contains("type") ||
        !data.contains("data") ||
        !data["data"].is_array() ||
        data["data"].size() != 1 ||
        !data["data"][0].contains("symbol") ||
        !data["data"][0].contains("asks") ||
        !data["data"][0].contains("bids") ||
        !data["data"][0].contains("checksum")) {
        ERROR("Invalid order book update: ", data.dump());
        return;
    }

    std::vector<PriceLevel> asks;
    std::vector<PriceLevel> bids;
    TradingPair pair = TradingPair::UNKNOWN;
    std::string symbol;

    bool isCompleteUpdate = (data["type"] == "snapshot") ? true : false;
        
    try {
        // keep the examples for ease of debugging. it will be easy to spot if the sample not parsed is not the same as the examples
        // example:
        // {"channel":"book","data":[{"asks":[{"price":93888.1,"qty":0.06918769},{"price":93888.2,"qty":0.0066583},{"price":93889.9,"qty":0.01161697},{"price":93890.2,"qty":0.84329594},{"price":93891.1,"qty":0.053},{"price":93892.7,"qty":0.01017287},{"price":93896.4,"qty":0.03184175},{"price":93898.0,"qty":5.325e-05},{"price":93901.8,"qty":0.135},{"price":93903.4,"qty":0.0339}],"bids":[{"price":93888.0,"qty":7.04391006},{"price":93887.5,"qty":3.08880155},{"price":93886.7,"qty":2.66278352},{"price":93886.5,"qty":5.072e-05},{"price":93886.2,"qty":1.63973},{"price":93886.1,"qty":2.76930693},{"price":93885.7,"qty":0.00803005},{"price":93885.0,"qty":0.10585564},{"price":93884.2,"qty":0.10650715},{"price":93882.6,"qty":5.3257969}],"checksum":2610022218,"symbol":"BTC/USD"}],"type":"snapshot"}

        symbol = data["data"][0]["symbol"].get<std::string>();
        pair = symbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            ERROR("Unknown trading pair: ", symbol, " - ", data.dump().substr(0, 300));
            return;
        }
        DEBUG("Processing order book ", data["type"], " for ", symbol, " - ", data.dump().substr(0, 3000));  
    } catch (const std::exception& e) {
        ERROR("Error processing order book update: ", e.what());
        return;
    }

    // Store previous state for debugging
    auto& book = m_orderBookManager.getOrderBook(ExchangeId::KRAKEN, pair);
    std::vector<PriceLevel> prevBids, prevAsks;
    uint32_t prevChecksum = 0;
    if (++countCalls % Config::KRAKEN_CHECKSUM_CHECK_PERIOD == 1) {
        prevBids = book.getBids();
        prevAsks = book.getAsks();
        prevChecksum = computeChecksum(buildChecksumString(pair, prevAsks) + buildChecksumString(pair, prevBids));
    }
    
    try {
        // Process asks
        for (const auto& ask : data["data"][0]["asks"]) {
            double price = ask["price"];
            double qty = ask["qty"];

            asks.push_back({price, qty});
        }

        // Process bids
        for (const auto& bid : data["data"][0]["bids"]) {
            double price = bid["price"];
            double qty = bid["qty"];
            
            bids.push_back({price, qty});
        }
    } catch (const std::exception& e) {
        ERROR("Error processing order book update: ", e.what());
        return;
    }

    try {
        m_orderBookManager.updateOrderBook(ExchangeId::KRAKEN, pair, bids, asks, isCompleteUpdate);

        uint32_t receivedChecksum = data["data"][0]["checksum"];
        // performance optimization as validation is slow. countCalls is increased above
        if (countCalls % Config::KRAKEN_CHECKSUM_CHECK_PERIOD == 1 && !isOrderBookValid(pair, receivedChecksum)) {
            // Get current state after update
            auto& currentBook = m_orderBookManager.getOrderBook(ExchangeId::KRAKEN, pair);
            auto currentBids = currentBook.getBids();
            auto currentAsks = currentBook.getAsks();
            std::string currentChecksumStr = buildChecksumString(pair, currentAsks) + buildChecksumString(pair, currentBids);
            uint32_t currentChecksum = computeChecksum(currentChecksumStr);

            // Print detailed debug info
            TRACE_CNT(CountableTrace::A_KRAKEN_ORDERBOOK_CHECKSUM_CHECK,    
                "Invalid order book checksum for ", symbol, "\n"
                "Previous checksum: ", prevChecksum, "\n"
                "Received checksum: ", receivedChecksum, "\n",  
                "Current checksum:  ", currentChecksum, "\n",
                "Previous asks: ", book.traceBidsAsks(prevAsks), 
                "\nPrevious bids: ", book.traceBidsAsks(prevBids), 
                "\nCurrent asks: ", book.traceBidsAsks(currentAsks), 
                "\nCurrent bids: ", book.traceBidsAsks(currentBids), 
                "\nUpdate data: ", data.dump());

            if (!resubscribeOrderBook({pair})) {
                ERROR("Failed to resubscribe after checksum mismatch for ", symbol);
                return;
            }
            ERROR_CNT(CountableTrace::A_KRAKEN_CHECKSUM_MISMATCH_RESTORED,
                "Resubscribed for ", symbol, " after checksum mismatch");
            return;
        }

        if (isCompleteUpdate) {
            // Mark that we have a snapshot for this symbol
            setSymbolSnapshotState(pair, true);
            TRACE("Got order book snapshot for ", symbol);
        }
    } catch (const std::exception& e) {
        ERROR("Error processing order book update: ", e.what());
    }
}

// CHECKSUM functions

// rules defined here: https://docs.kraken.com/api/docs/guides/spot-ws-book-v2
// Remove the decimal character, '.', from the price, i.e. "45285.2" -> "452852".
// Remove all leading zero characters from the price. i.e. "452852" -> "452852".
std::string ApiKraken::formatPrice(TradingPair pair, double price) {
    std::ostringstream out;
    // Get the trading pair from the current order book being processed
    int precision = getPricePrecision(pair);
    out << std::fixed << std::setprecision(precision) << price;
    std::string s = out.str();
    s.erase(std::remove(s.begin(), s.end(), '.'), s.end());
    s.erase(0, s.find_first_not_of('0'));
    return s;
}

// Remove the decimal character, '.', from the qty, i.e. "0.00100000" -> "000100000".
// Remove all leading zero characters from the qty. i.e. "000100000" -> "100000".
std::string ApiKraken::formatQty(double qty) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(8) << qty;   
    std::string s = out.str();
    s.erase(std::remove(s.begin(), s.end(), '.'), s.end());
    s.erase(0, s.find_first_not_of('0'));
    return s;
}

std::string ApiKraken::buildChecksumString(TradingPair pair, const std::vector<PriceLevel>& prices) {
    std::ostringstream oss;
    
    // Process asks (sorted by price from low to high)
    size_t depth = std::min(size_t(10), prices.size());
    for (size_t i = 0; i < depth; ++i) {
        oss << formatPrice(pair, prices[i].price) << formatQty(prices[i].quantity);
    }

    return oss.str();
}

uint32_t ApiKraken::computeChecksum(const std::string& checksumString) {
    return crc32(0L, reinterpret_cast<const unsigned char*>(checksumString.c_str()), checksumString.length());
}

bool ApiKraken::isOrderBookValid(TradingPair pair, uint32_t receivedChecksum) {
    auto& book = m_orderBookManager.getOrderBook(ExchangeId::KRAKEN, pair);
    auto bids = book.getBids();
    auto asks = book.getAsks();
    std::string str = buildChecksumString(pair, asks) + buildChecksumString(pair, bids);
    uint32_t localChecksum = computeChecksum(str);
    
    if (localChecksum != receivedChecksum) {
        TRACE_CNT(CountableTrace::A_KRAKEN_ORDERBOOK_CHECKSUM_CHECK2,
            "Invalid order book checksum: ", receivedChecksum, " local: ", localChecksum, " for ", pair, " - [", str, "] ");
        return false;
    } else {
        TRACE_CNT(CountableTrace::A_KRAKEN_ORDERBOOK_CHECKSUM_CHECK_OK, "[", pair, "]",
            "Valid order book checksum: ", receivedChecksum);
    }
    return true;
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
        ERROR("Error placing order: ", e.what());
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
        ERROR("Error cancelling order: ", e.what());
        return false;
    }
}

bool ApiKraken::getBalance(const std::string& asset) {
    if (!m_connected) {
        ERROR("Not connected to Kraken");
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
        ERROR("Error getting balance: ", e.what());
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
