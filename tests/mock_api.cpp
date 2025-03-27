#include "mock_api.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <condition_variable>

MockApi::MockApi(OrderBookManager& orderBookManager, const std::string& name)
    : m_name(name)
    , m_id(name == "Binance" ? ExchangeId::BINANCE : ExchangeId::KRAKEN)
    , m_orderBookManager(orderBookManager) {
    
    std::cout << "[" << getTestTimestamp() << "] MockApi: Creating " << m_name << " API..." << std::endl << std::flush;
    
    // Initialize symbol map
    std::cout << "[" << getTestTimestamp() << "] MockApi: Initializing symbol map..." << std::endl << std::flush;
    m_symbolMap[TradingPair::BTC_USDT] = name == "Binance" ? "BTCUSDT" : "XBT/USD";
    m_symbolMap[TradingPair::ETH_USDT] = name == "Binance" ? "ETHUSDT" : "ETH/USD";
    m_symbolMap[TradingPair::XTZ_USDT] = name == "Binance" ? "XTZUSDT" : "XTZ/USD";
    std::cout << "[" << getTestTimestamp() << "] MockApi: Symbol map initialized" << std::endl << std::flush;
}

MockApi::~MockApi() {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Destroying " << m_name << " API..." << std::endl;
    disconnect();
}

bool MockApi::connect() {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Starting connect to " << m_name << "..." << std::endl;
    if (m_connected) {
        std::cout << "[" << getTestTimestamp() << "] MockApi: Already connected" << std::endl;
        return true;
    }

    // Simulate connection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_connected = true;
    std::cout << "[" << getTestTimestamp() << "] MockApi: Connected successfully" << std::endl;
    return true;
}

void MockApi::disconnect() {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Starting disconnect from " << m_name << "..." << std::endl;
    if (!m_connected) {
        std::cout << "[" << getTestTimestamp() << "] MockApi: Already disconnected" << std::endl;
        return;
    }

    // Simulate disconnection delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    m_connected = false;
    m_subscribed = false;
    std::cout << "[" << getTestTimestamp() << "] MockApi: Disconnected successfully" << std::endl;
}

bool MockApi::subscribeOrderBook(TradingPair pair) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Starting subscribe to order book for " << toString(pair) << "..." << std::endl;
    if (!m_connected) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Not connected" << std::endl;
        return false;
    }

    try {
        std::string symbol = tradingPairToSymbol(pair);
        
        // Simulate subscription delay
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Simulate successful subscription response
        json response = {
            {"event", "subscribe"},
            {"status", "success"},
            {"pair", symbol},
            {"subscription", {
                {"name", "book"}
            }}
        };

        std::cout << "[" << getTestTimestamp() << "] MockApi: Processing subscription response: " << response.dump() << std::endl;
        handleMockMessage(response.dump());
        
        m_subscribed = true;
        if (m_subscriptionCallback) {
            std::cout << "[" << getTestTimestamp() << "] MockApi: Calling subscription callback with success" << std::endl;
            m_subscriptionCallback(true);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Error in subscribeOrderBook: " << e.what() << std::endl;
        if (m_subscriptionCallback) {
            std::cout << "[" << getTestTimestamp() << "] MockApi: Calling subscription callback with failure" << std::endl;
            m_subscriptionCallback(false);
        }
        return false;
    }
}

bool MockApi::getOrderBookSnapshot(TradingPair pair) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Getting order book snapshot for " << toString(pair) << "..." << std::endl;
    if (!m_connected) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Not connected" << std::endl;
        return false;
    }

    try {
        // Simulate snapshot delay
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // Simulate a snapshot with some test data
        std::vector<PriceLevel> bids = {
            {50000.0, 1.0},
            {49999.0, 2.0},
            {49998.0, 3.0}
        };

        std::vector<PriceLevel> asks = {
            {50001.0, 1.0},
            {50002.0, 2.0},
            {50003.0, 3.0}
        };

        std::cout << "[" << getTestTimestamp() << "] MockApi: Updating order book with snapshot data..." << std::endl;
        for (const auto& bid : bids) {
            m_orderBookManager.updateOrderBook(m_id, pair, bid.price, bid.quantity, true);
        }

        for (const auto& ask : asks) {
            m_orderBookManager.updateOrderBook(m_id, pair, ask.price, ask.quantity, false);
        }

        if (m_snapshotCallback) {
            std::cout << "[" << getTestTimestamp() << "] MockApi: Calling snapshot callback with success" << std::endl;
            m_snapshotCallback(true);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Error in getOrderBookSnapshot: " << e.what() << std::endl;
        if (m_snapshotCallback) {
            std::cout << "[" << getTestTimestamp() << "] MockApi: Calling snapshot callback with failure" << std::endl;
            m_snapshotCallback(false);
        }
        return false;
    }
}

bool MockApi::placeOrder(TradingPair pair, OrderType type, double price, double quantity) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Placing " << toString(type) << " order for " << toString(pair) 
              << " at price " << price << " and quantity " << quantity << std::endl;
    if (!m_connected) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Not connected" << std::endl;
        return false;
    }
    // Simulate order placement delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

bool MockApi::cancelOrder(const std::string& orderId) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Canceling order " << orderId << std::endl;
    if (!m_connected) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Not connected" << std::endl;
        return false;
    }
    // Simulate order cancellation delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

bool MockApi::getBalance(const std::string& asset) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Getting balance for " << asset << std::endl;
    if (!m_connected) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Not connected" << std::endl;
        return false;
    }
    // Simulate balance check delay
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    return true;
}

void MockApi::setSubscriptionCallback(std::function<void(bool)> callback) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Setting subscription callback" << std::endl;
    m_subscriptionCallback = callback;
}

void MockApi::setSnapshotCallback(std::function<void(bool)> callback) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Setting snapshot callback" << std::endl;
    m_snapshotCallback = callback;
}

TradingPair MockApi::symbolToTradingPair(const std::string& symbol) const {
    std::string lowerSymbol = toLower(symbol);
    
    if (m_name == "Binance") {
        if (lowerSymbol == "btcusdt") return TradingPair::BTC_USDT;
        if (lowerSymbol == "ethusdt") return TradingPair::ETH_USDT;
        if (lowerSymbol == "xtzusdt") return TradingPair::XTZ_USDT;
    } else {
        if (lowerSymbol == "xbt/usd" || lowerSymbol == "btc/usd") return TradingPair::BTC_USDT;
        if (lowerSymbol == "eth/usd") return TradingPair::ETH_USDT;
        if (lowerSymbol == "xtz/usd") return TradingPair::XTZ_USDT;
    }
    
    return TradingPair::UNKNOWN;
}

std::string MockApi::tradingPairToSymbol(TradingPair pair) {
    auto it = m_symbolMap.find(pair);
    if (it != m_symbolMap.end()) {
        return it->second;
    }
    throw std::runtime_error("Unknown trading pair");
}

void MockApi::simulateOrderBookUpdate(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Simulating order book update..." << std::endl;
    if (!m_subscribed) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Not subscribed to order book updates" << std::endl;
        return;
    }

    json update = {
        {"e", "depthUpdate"},
        {"s", tradingPairToSymbol(TradingPair::BTC_USDT)},
        {"b", json::array()},
        {"a", json::array()}
    };

    for (const auto& bid : bids) {
        update["b"].push_back({std::to_string(bid.price), std::to_string(bid.quantity)});
    }

    for (const auto& ask : asks) {
        update["a"].push_back({std::to_string(ask.price), std::to_string(ask.quantity)});
    }

    std::cout << "[" << getTestTimestamp() << "] MockApi: Processing update message: " << update.dump() << std::endl;
    handleMockMessage(update.dump());
}

void MockApi::handleMockMessage(const std::string& msg) {
    std::cout << "[" << getTestTimestamp() << "] MockApi: Processing message: " << msg << std::endl;
    try {
        json data = json::parse(msg);
        if (data.contains("event") && data["event"] == "subscribe") {
            if (m_subscriptionCallback) {
                std::cout << "[" << getTestTimestamp() << "] MockApi: Calling subscription callback with success" << std::endl;
                m_subscriptionCallback(true);
            }
        } else if (data.contains("e") && data["e"] == "depthUpdate") {
            std::string symbol = data["s"];
            TradingPair pair = symbolToTradingPair(symbol);
            
            // Process bids
            for (const auto& bid : data["b"]) {
                double price = std::stod(bid[0].get<std::string>());
                double quantity = std::stod(bid[1].get<std::string>());
                std::cout << "[" << getTestTimestamp() << "] MockApi: Updating bid: price=" << price << ", quantity=" << quantity << std::endl;
                m_orderBookManager.updateOrderBook(m_id, pair, price, quantity, true);
            }
            
            // Process asks
            for (const auto& ask : data["a"]) {
                double price = std::stod(ask[0].get<std::string>());
                double quantity = std::stod(ask[1].get<std::string>());
                std::cout << "[" << getTestTimestamp() << "] MockApi: Updating ask: price=" << price << ", quantity=" << quantity << std::endl;
                m_orderBookManager.updateOrderBook(m_id, pair, price, quantity, false);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Error processing message: " << e.what() << std::endl;
    }
} 