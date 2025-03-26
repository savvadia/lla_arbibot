#include "mock_api.h"
#include <iostream>
#include <thread>

MockApi::MockApi(OrderBookManager& orderBookManager, const std::string& name)
    : m_name(name)
    , m_id(name == "Binance" ? ExchangeId::BINANCE : ExchangeId::KRAKEN)
    , m_orderBookManager(orderBookManager) {
    
    // Initialize symbol map
    m_symbolMap[TradingPair::BTC_USDT] = name == "Binance" ? "BTCUSDT" : "XBT/USD";
    m_symbolMap[TradingPair::ETH_USDT] = name == "Binance" ? "ETHUSDT" : "ETH/USD";
    m_symbolMap[TradingPair::XTZ_USDT] = name == "Binance" ? "XTZUSDT" : "XTZ/USD";

    // Create WebSocket server
    m_server = std::make_unique<MockWebSocketServer>(m_ioc);
    m_server->setOnConnect([this]() { onServerConnect(); });
    m_server->setOnMessage([this](const std::string& msg) { onServerMessage(msg); });

    // Start the io_context thread
    m_ioThread = std::thread([this]() {
        m_ioc.run();
    });
}

MockApi::~MockApi() {
    disconnect();
    m_ioc.stop();
    if (m_ioThread.joinable()) {
        m_ioThread.join();
    }
}

bool MockApi::connect() {
    if (m_connected) return true;

    try {
        // Start the server
        m_server->start();
        
        // Give the server a moment to start listening
        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        // Create a client WebSocket
        auto socket = tcp::socket(m_ioc);
        tcp::endpoint endpoint(net::ip::make_address("127.0.0.1"), m_server->getPort());
        socket.connect(endpoint);

        m_ws = std::make_unique<websocket::stream<tcp::socket>>(std::move(socket));

        // WebSocket handshake
        m_ws->handshake("localhost", "/");

        m_connected = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in connect: " << e.what() << std::endl;
        return false;
    }
}

void MockApi::disconnect() {
    if (!m_connected) return;
    
    if (m_ws) {
        try {
            m_ws->close(websocket::close_code::normal);
        } catch (const std::exception& e) {
            std::cerr << "Error in disconnect: " << e.what() << std::endl;
        }
        m_ws.reset();
    }

    m_server->stop();
    m_connected = false;
}

bool MockApi::subscribeOrderBook(TradingPair pair) {
    if (!m_connected) {
        std::cerr << "Not connected to " << m_name << std::endl;
        return false;
    }

    auto it = m_symbolMap.find(pair);
    if (it == m_symbolMap.end()) {
        std::cerr << "Unsupported trading pair" << std::endl;
        return false;
    }

    json subscription = {
        {"method", "SUBSCRIBE"},
        {"params", {it->second + "@depth@100ms"}},
        {"id", 1}
    };

    try {
        m_ws->write(net::buffer(subscription.dump()));
        if (m_subscriptionCallback) {
            m_subscriptionCallback(true);
        }
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error in subscribe: " << e.what() << std::endl;
        return false;
    }
}

bool MockApi::getOrderBookSnapshot(TradingPair pair) {
    if (!m_connected) {
        std::cerr << "Not connected to " << m_name << std::endl;
        return false;
    }

    auto it = m_symbolMap.find(pair);
    if (it == m_symbolMap.end()) {
        std::cerr << "Unsupported trading pair" << std::endl;
        return false;
    }

    // Send a mock snapshot
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

    sendOrderBookSnapshot(bids, asks);
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

std::string MockApi::tradingPairToSymbol(TradingPair pair) const {
    auto it = m_symbolMap.find(pair);
    if (it != m_symbolMap.end()) {
        return it->second;
    }
    throw std::runtime_error("Unsupported trading pair");
}

void MockApi::sendOrderBookUpdate(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    json update = {
        {"e", "depthUpdate"},
        {"s", "BTCUSDT"},
        {"b", json::array()},
        {"a", json::array()}
    };

    for (const auto& bid : bids) {
        update["b"].push_back({std::to_string(bid.price), std::to_string(bid.quantity)});
    }

    for (const auto& ask : asks) {
        update["a"].push_back({std::to_string(ask.price), std::to_string(ask.quantity)});
    }

    m_server->sendMessage(update.dump());
}

void MockApi::sendOrderBookSnapshot(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
    json snapshot = {
        {"lastUpdateId", 123456},
        {"bids", json::array()},
        {"asks", json::array()}
    };

    for (const auto& bid : bids) {
        snapshot["bids"].push_back({std::to_string(bid.price), std::to_string(bid.quantity)});
    }

    for (const auto& ask : asks) {
        snapshot["asks"].push_back({std::to_string(ask.price), std::to_string(ask.quantity)});
    }

    m_server->sendMessage(snapshot.dump());
}

void MockApi::onServerConnect() {
    m_connected = true;
}

void MockApi::onServerMessage(const std::string& message) {
    processMessage(message);
}

void MockApi::processMessage(const std::string& message) {
    try {
        json data = json::parse(message);
        
        // Process subscription response
        if (data.contains("result") && data["result"] == nullptr) {
            return;
        }

        // Process order book update
        if (data.contains("e") && data["e"] == "depthUpdate") {
            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;

            if (data.contains("b")) {
                for (const auto& bid : data["b"]) {
                    bids.push_back({
                        std::stod(bid[0].get<std::string>()),
                        std::stod(bid[1].get<std::string>())
                    });
                }
            }

            if (data.contains("a")) {
                for (const auto& ask : data["a"]) {
                    asks.push_back({
                        std::stod(ask[0].get<std::string>()),
                        std::stod(ask[1].get<std::string>())
                    });
                }
            }

            m_orderBookManager.updateOrderBook(getExchangeId(), TradingPair::BTC_USDT, bids, asks);
            if (m_updateCallback) {
                m_updateCallback();
            }
        }
        // Process order book snapshot
        else if (data.contains("lastUpdateId")) {
            std::vector<PriceLevel> bids;
            std::vector<PriceLevel> asks;

            for (const auto& bid : data["bids"]) {
                bids.push_back({
                    std::stod(bid[0].get<std::string>()),
                    std::stod(bid[1].get<std::string>())
                });
            }

            for (const auto& ask : data["asks"]) {
                asks.push_back({
                    std::stod(ask[0].get<std::string>()),
                    std::stod(ask[1].get<std::string>())
                });
            }

            m_orderBookManager.updateOrderBook(getExchangeId(), TradingPair::BTC_USDT, bids, asks);
            if (m_snapshotCallback) {
                m_snapshotCallback(true);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "Error processing message: " << e.what() << std::endl;
    }
} 