#pragma once

#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

#include "api_exchange.h"

using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

class ApiBinance : public ApiExchange {
public:
    ApiBinance(const std::vector<TradingPair> pairs, bool testMode = true);

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook() override;
    bool resubscribeOrderBook(const std::vector<TradingPair>& pairs) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;

    // Process messages for all exchanges
    void processBookTicker(const json& data);

    // Order management
    bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) override;
    bool cancelOrder(const std::string& orderId) override;
    bool getBalance(const std::string& asset) override;

    std::string getExchangeName() const override { return toString(ExchangeId::BINANCE); }
    ExchangeId getExchangeId() const override { return ExchangeId::BINANCE; }

protected:
    // Override the cooldown method for Binance-specific rate limiting
    void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "") override;
    
    // Implement pure virtual methods from base class
    void processRateLimitHeaders(const std::string& headers) override;

    // Message processing methods
    void processMessage(const json& data) override;
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);
}; 