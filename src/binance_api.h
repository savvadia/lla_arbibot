#ifndef BINANCE_API_H
#define BINANCE_API_H

#include "orderbook.h"
#include <string>
#include <functional>
#include <memory>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_no_tls_client.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class BinanceApi {
public:
    BinanceApi(OrderBookManager& orderBookManager);
    ~BinanceApi();

    // Initialize WebSocket connection
    void connect();
    void disconnect();

    // Subscribe to order book updates for a trading pair
    void subscribeOrderBook(TradingPair pair);

    // Get current order book snapshot
    void getOrderBookSnapshot(TradingPair pair);

private:
    // WebSocket callbacks
    void onOpen(websocketpp::connection_hdl hdl);
    void onClose(websocketpp::connection_hdl hdl);
    void onMessage(websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg);
    void onError(websocketpp::connection_hdl hdl);

    // Process order book updates
    void processOrderBookUpdate(const json& data);
    void processOrderBookSnapshot(const json& data);

    // Convert Binance symbol to our TradingPair
    TradingPair binanceSymbolToTradingPair(const std::string& symbol);

    OrderBookManager& orderBookManager;
    websocketpp::client<websocketpp::config::asio_client> client;
    websocketpp::connection_hdl connection;
    bool connected;
    std::map<std::string, TradingPair> symbolToPair;
};

#endif // BINANCE_API_H 