#include "binance_api.h"
#include "tracer.h"
#include <sstream>
#include <iostream>

BinanceApi::BinanceApi(OrderBookManager& orderBookManager) 
    : orderBookManager(orderBookManager), connected(false) {
    
    // Initialize symbol to trading pair mapping
    symbolToPair["BTCUSDT"] = TradingPair::BTC_USDT;
    symbolToPair["ETHUSDT"] = TradingPair::ETH_USDT;
    symbolToPair["XTZUSDT"] = TradingPair::XTZ_USDT;

    // Initialize WebSocket client
    client.init_asio();
    client.set_open_handler(std::bind(&BinanceApi::onOpen, this, std::placeholders::_1));
    client.set_close_handler(std::bind(&BinanceApi::onClose, this, std::placeholders::_1));
    client.set_message_handler(std::bind(&BinanceApi::onMessage, this, std::placeholders::_1, std::placeholders::_2));
    client.set_error_handler(std::bind(&BinanceApi::onError, this, std::placeholders::_1));
}

BinanceApi::~BinanceApi() {
    disconnect();
}

void BinanceApi::connect() {
    if (connected) return;

    try {
        websocketpp::lib::error_code ec;
        client::connection_ptr con = client.get_connection("ws://" + m_host + ":" + m_port + "/ws", ec);
        if (ec) {
            TRACE("BINANCE", "Failed to create connection: %s", ec.message().c_str());
            return;
        }

        client.connect(con);
        connection = con->get_handle();
        connected = true;
        TRACE("BINANCE", "Connected to Binance WebSocket");
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Exception during connection: %s", e.what());
    }
}

void BinanceApi::disconnect() {
    if (!connected) return;

    try {
        client.close(connection, websocketpp::close::status::normal, "Disconnecting");
        connected = false;
        TRACE("BINANCE", "Disconnected from Binance WebSocket");
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Exception during disconnection: %s", e.what());
    }
}

void BinanceApi::subscribeOrderBook(TradingPair pair) {
    if (!connected) {
        TRACE("BINANCE", "Not connected, subscription will be queued");
        return;
    }

    try {
        std::string symbol;
        for (const auto& [sym, p] : symbolToPair) {
            if (p == pair) {
                symbol = sym;
                break;
            }
        }

        if (symbol.empty()) {
            TRACE("BINANCE", "Invalid trading pair for subscription");
            return;
        }

        // Subscribe to order book updates
        json subscribeMsg = {
            {"method", "SUBSCRIBE"},
            {"params", {
                symbol + "@depth@100ms"
            }},
            {"id", 1}
        };

        client.send(connection, subscribeMsg.dump(), websocketpp::frame::opcode::text);
        TRACE("BINANCE", "Subscribed to order book for %s", symbol.c_str());
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Exception during subscription: %s", e.what());
    }
}

void BinanceApi::getOrderBookSnapshot(TradingPair pair) {
    if (!connected) {
        TRACE("BINANCE", "Not connected, cannot get snapshot");
        return;
    }

    std::string symbol;
    for (const auto& [sym, p] : symbolToPair) {
        if (p == pair) {
            symbol = sym;
            break;
        }
    }

    if (symbol.empty()) {
        TRACE("BINANCE", "Invalid trading pair for snapshot");
        return;
    }

    // TODO: Implement REST API call to get order book snapshot
    // This will require adding HTTP client library (e.g., libcurl)
    TRACE("BINANCE", "Getting order book snapshot for %s", symbol.c_str());
}

void BinanceApi::onOpen(websocketpp::connection_hdl hdl) {
    connection = hdl;
    connected = true;
    TRACE("BINANCE", "WebSocket connection opened");
}

void BinanceApi::onClose(websocketpp::connection_hdl hdl) {
    connected = false;
    TRACE("BINANCE", "WebSocket connection closed");
}

void BinanceApi::onMessage(websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
    try {
        json data = json::parse(msg->get_payload());
        processOrderBookUpdate(data);
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Error processing message: %s", e.what());
    }
}

void BinanceApi::onError(websocketpp::connection_hdl hdl) {
    TRACE("BINANCE", "WebSocket error occurred");
}

void BinanceApi::processOrderBookUpdate(const json& data) {
    try {
        if (!data.contains("e") || data["e"] != "depthUpdate") {
            return;
        }

        std::string symbol = data["s"].get<std::string>();
        TradingPair pair = binanceSymbolToTradingPair(symbol);
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

        orderBookManager.updateOrderBook(ExchangeId::BINANCE, pair, bids, asks);
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Error processing order book update: %s", e.what());
    }
}

void BinanceApi::processOrderBookSnapshot(const json& data) {
    try {
        if (!data.contains("symbol")) {
            return;
        }

        std::string symbol = data["symbol"].get<std::string>();
        TradingPair pair = binanceSymbolToTradingPair(symbol);
        if (pair == TradingPair::UNKNOWN) {
            return;
        }

        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : data["bids"]) {
            bids.push_back({
                std::stod(bid[0].get<std::string>()),  // price
                std::stod(bid[1].get<std::string>())   // quantity
            });
        }

        // Process asks
        for (const auto& ask : data["asks"]) {
            asks.push_back({
                std::stod(ask[0].get<std::string>()),  // price
                std::stod(ask[1].get<std::string>())   // quantity
            });
        }

        orderBookManager.updateOrderBook(ExchangeId::BINANCE, pair, bids, asks);
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Error processing order book snapshot: %s", e.what());
    }
}

TradingPair BinanceApi::binanceSymbolToTradingPair(const std::string& symbol) {
    auto it = symbolToPair.find(symbol);
    if (it != symbolToPair.end()) {
        return it->second;
    }
    return TradingPair::UNKNOWN;
} 