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

class ApiKucoin : public ApiExchange {
public:
    ApiKucoin(OrderBookManager& orderBookManager, TimersMgr& timersMgr,
        const std::vector<TradingPair> pairs, bool testMode = true);

    bool connect() override;
    bool initWebSocketEndpoint();
    void sendPing();
    void startPingTimer();

    // Subscribe to order book updates for a trading pair
    bool subscribeOrderBook() override;
    bool resubscribeOrderBook(const std::vector<TradingPair>& pairs) override;

    // Get current order book snapshot
    bool getOrderBookSnapshot(TradingPair pair) override;


    // Order management
    bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) override;
    bool cancelOrder(const std::string& orderId) override;
    bool getBalance(const std::string& asset) override;

    std::string getExchangeName() const override { return "KUCOIN"; }
    ExchangeId getExchangeId() const override { return ExchangeId::KUCOIN; }

    int m_pingIntervalMs = 18000;
    int m_pingTimeoutMs = 10000;
    int m_pingTimerId;

    std::string m_token;

protected:
    // Override the cooldown method for KUCOIN-specific rate limiting
    void cooldown(int httpCode, const std::string& response, const std::string& endpoint = "") override;
    
    // Implement pure virtual methods from base class
    void processRateLimitHeaders(const std::string& headers) override;

    // Message processing methods
    // Process messages for all exchanges
    void processMessage(const json& data) override;
    void processLevel1(const json& data);
    void processSubscribeResponse(const json& data);
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data, TradingPair pair);
}; 