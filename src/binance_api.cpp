#include "binance_api.h"
#include "tracer.h"
#include <sstream>
#include <iostream>

BinanceApi::BinanceApi(OrderBookManager& orderBookManager) 
    : orderBookManager(orderBookManager), connected(false) {
    
    // Initialize symbol to trading pair mapping
    symbolToPair["BTCUSDT"] = TradingPair::BTC_USDT;
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
        client.connect(ec);
        if (ec) {
            TRACE("BINANCE", "Failed to connect: %s", ec.message().c_str());
            return;
        }

        // Start the ASIO io_service run loop
        client.run();
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
    if (!connected) return;

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
    TRACE("BINANCE", "WebSocket connection opened");
}

void BinanceApi::onClose(websocketpp::connection_hdl hdl) {
    connected = false;
    TRACE("BINANCE", "WebSocket connection closed");
}

void BinanceApi::onMessage(websocketpp::connection_hdl hdl, websocketpp::config::asio_client::message_type::ptr msg) {
    try {
        json data = json::parse(msg->get_payload());
        
        // Check if this is an order book update
        if (data.contains("e") && data["e"] == "depthUpdate") {
            processOrderBookUpdate(data);
        }
        // Check if this is an order book snapshot
        else if (data.contains("lastUpdateId")) {
            processOrderBookSnapshot(data);
        }
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Exception processing message: %s", e.what());
    }
}

void BinanceApi::onError(websocketpp::connection_hdl hdl) {
    TRACE("BINANCE", "WebSocket error occurred");
}

void BinanceApi::processOrderBookUpdate(const json& data) {
    try {
        std::string symbol = data["s"];
        TradingPair pair = binanceSymbolToTradingPair(symbol);
        
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : data["b"]) {
            PriceLevel level;
            level.price = std::stod(bid[0].get<std::string>());
            level.quantity = std::stod(bid[1].get<std::string>());
            if (level.quantity > 0) {  // Only add non-zero quantity levels
                bids.push_back(level);
            }
        }

        // Process asks
        for (const auto& ask : data["a"]) {
            PriceLevel level;
            level.price = std::stod(ask[0].get<std::string>());
            level.quantity = std::stod(ask[1].get<std::string>());
            if (level.quantity > 0) {  // Only add non-zero quantity levels
                asks.push_back(level);
            }
        }

        // Update order book
        orderBookManager.updateOrderBook(pair, bids, asks);
        TRACE("BINANCE", "Updated order book for %s", symbol.c_str());
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Exception processing order book update: %s", e.what());
    }
}

void BinanceApi::processOrderBookSnapshot(const json& data) {
    try {
        std::string symbol = data["symbol"];
        TradingPair pair = binanceSymbolToTradingPair(symbol);
        
        std::vector<PriceLevel> bids;
        std::vector<PriceLevel> asks;

        // Process bids
        for (const auto& bid : data["bids"]) {
            PriceLevel level;
            level.price = std::stod(bid[0].get<std::string>());
            level.quantity = std::stod(bid[1].get<std::string>());
            if (level.quantity > 0) {  // Only add non-zero quantity levels
                bids.push_back(level);
            }
        }

        // Process asks
        for (const auto& ask : data["asks"]) {
            PriceLevel level;
            level.price = std::stod(ask[0].get<std::string>());
            level.quantity = std::stod(ask[1].get<std::string>());
            if (level.quantity > 0) {  // Only add non-zero quantity levels
                asks.push_back(level);
            }
        }

        // Update order book
        orderBookManager.updateOrderBook(pair, bids, asks);
        TRACE("BINANCE", "Updated order book snapshot for %s", symbol.c_str());
    } catch (const std::exception& e) {
        TRACE("BINANCE", "Exception processing order book snapshot: %s", e.what());
    }
}

TradingPair BinanceApi::binanceSymbolToTradingPair(const std::string& symbol) {
    auto it = symbolToPair.find(symbol);
    return it != symbolToPair.end() ? it->second : TradingPair::MAX;
} 