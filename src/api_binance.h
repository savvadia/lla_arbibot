#pragma once

#include "api_exchange.h"
#include "orderbook_mgr.h"
#include "timers.h"
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/ssl.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <boost/asio/strand.hpp>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

using tcp = boost::asio::ip::tcp;
using json = nlohmann::json;

class ApiBinance : public ApiExchange {
public:
    ApiBinance(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
        const std::vector<TradingPair> pairs, bool testMode = true);

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

    std::string getExchangeName() const override { return "BINANCE"; }
    ExchangeId getExchangeId() const override { return ExchangeId::BINANCE; }

protected:
    // Override the cooldown method for Binance-specific rate limiting
    void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "") override;
    
    // Implement pure virtual methods from base class
    void processRateLimitHeaders(const std::string& headers) override;

    // Message processing methods
    void processMessage(const std::string& message) override;
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);
    
    // Internal symbol conversion methods
    TradingPair symbolToTradingPair(const std::string& symbol) const;
    std::string tradingPairToSymbol(TradingPair pair) const;
}; 