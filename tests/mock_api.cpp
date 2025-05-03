#include "mock_api.h"
#include <iostream>
#include <chrono>
#include "test_utils.h"
#include <thread>
#include <nlohmann/json.hpp>

MockApi::MockApi(OrderBookManager& orderBookManager, TimersMgr& timersMgr, const std::string& name, bool testMode)
    : ApiExchange(orderBookManager, timersMgr, 
    name == "Binance" ? "https://api.binance.com/api/v3" : "https://api.kraken.com/0/public",
    testMode)
    , m_name(name)
    , m_id(name == "Binance" ? ExchangeId::BINANCE : ExchangeId::KRAKEN) {
    
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

bool MockApi::subscribeOrderBook(std::vector<TradingPair> pairs) {
    std::string symbols = "";
    for (const auto& pair : pairs) {
        symbols += tradingPairToSymbol(pair) + ",";
    }

    std::cout << "[" << getTestTimestamp() << "] MockApi: Starting subscribe to order book for " << symbols << "..." << std::endl;
    if (!m_connected) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Not connected" << std::endl;
        return false;
    }

    try {
        std::vector<std::string> symbols;
        for (const auto& pair : pairs) {
            symbols.push_back(tradingPairToSymbol(pair));
        }
        
        // Simulate subscription delay
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Simulate successful subscription response
        json response = {
            {"event", "subscribe"},
            {"status", "success"},
            {"pair", symbols},
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
        m_orderBookManager.updateOrderBook(m_id, pair, bids, asks);

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
    
    // Create a mock update message
    json update = {
        {"type", "book"},
        {"pair", "BTCUSDT"},
        {"bids", json::array()},
        {"asks", json::array()}
    };

    // Add bids and asks to the message
    for (const auto& bid : bids) {
        update["bids"].push_back({std::to_string(bid.price), std::to_string(bid.quantity)});
    }
    for (const auto& ask : asks) {
        update["asks"].push_back({std::to_string(ask.price), std::to_string(ask.quantity)});
    }

    // Process the mock message
    handleMockMessage(update.dump());
}

void MockApi::handleMockMessage(const std::string& msg) {
    try {
        json data = json::parse(msg);
        
        // Handle subscription response
        if (data.contains("event") && data["event"] == "subscribe") {
            if (data.contains("status") && data["status"] == "success") {
                std::cout << "[" << getTestTimestamp() << "] MockApi: Subscription successful" << std::endl;
                m_subscribed = true;
                if (m_subscriptionCallback) {
                    m_subscriptionCallback(true);
                }
            } else {
                std::cout << "[" << getTestTimestamp() << "] MockApi: Subscription failed" << std::endl;
                if (m_subscriptionCallback) {
                    m_subscriptionCallback(false);
                }
            }
            return;
        }

        // Handle order book update
        if (data.contains("type") && data["type"] == "book") {
            TradingPair pair = symbolToTradingPair(data["pair"]);
            if (pair == TradingPair::UNKNOWN) {
                std::cerr << "[" << getTestTimestamp() << "] MockApi: Unknown trading pair" << std::endl;
                return;
            }

            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;

            // Process bids
            if (data.contains("bids")) {
                for (const auto& bid : data["bids"]) {
                    double price = std::stod(bid[0].get<std::string>());
                    double quantity = std::stod(bid[1].get<std::string>());
                    bids.push_back({price, quantity});
                }
            }

            // Process asks
            if (data.contains("asks")) {
                for (const auto& ask : data["asks"]) {
                    double price = std::stod(ask[0].get<std::string>());
                    double quantity = std::stod(ask[1].get<std::string>());
                    asks.push_back({price, quantity});
                }
            }

            if (!bids.empty() || !asks.empty()) {
                m_orderBookManager.updateOrderBook(m_id, pair, bids, asks);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[" << getTestTimestamp() << "] MockApi: Error processing message: " << e.what() << std::endl;
    }
}

void MockApi::processMessages() {
    // All messages are processed asynchronously via callbacks in tests
}

void MockApi::processRateLimitHeaders(const std::string& headers) {
    // Mock implementation - no actual rate limiting in tests
    std::cout << "[" << getTestTimestamp() << "] MockApi: Processing rate limit headers (mock)" << std::endl;
}
