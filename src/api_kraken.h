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

class ApiKraken : public ApiExchange {
public:
    ApiKraken(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
        const std::vector<TradingPair> pairs, bool testMode = false);

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook() override;
    bool handleSubscribeUnsubscribe(const std::vector<TradingPair>& pairs, bool subscribe);
    bool resubscribeOrderBook(const std::vector<TradingPair>& pairs) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;

    // Order management
    bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) override;
    bool cancelOrder(const std::string& orderId) override;
    bool getBalance(const std::string& asset) override;

    std::string getExchangeName() const override { return "KRAKEN"; }
    ExchangeId getExchangeId() const override { return ExchangeId::KRAKEN; }

    // Helper functions for checksum calculation
    std::string formatPrice(TradingPair pair, double price);
    std::string formatQty(double qty);
    std::string buildChecksumString(TradingPair pair, const std::vector<PriceLevel>& prices);
    uint32_t computeChecksum(const std::string& checksumString);
    bool isOrderBookValid(TradingPair pair, uint32_t receivedChecksum);

protected:
    // Override the cooldown method for Kraken-specific rate limiting
    void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "") override;
    
    // Implement pure virtual methods from base class
    void processRateLimitHeaders(const std::string& headers) override;
    
    // WebSocket callbacks
    void processMessage(const json& data) override;
    void processOrderBookUpdate(const json& data);

private:
    // Kraken-specific message handlers
    void processTickerUpdate(const json& data);
}; 