#pragma once

#include "../src/exchange_api.h"
#include "../src/orderbook.h"
#include "mock_websocket_server.h"
#include <memory>
#include <functional>
#include <map>
#include <thread>
#include <boost/asio/io_context.hpp>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class MockApi : public ExchangeApi {
public:
    MockApi(OrderBookManager& orderBookManager, const std::string& name);
    ~MockApi();

    bool connect() override;
    void disconnect() override;
    bool subscribeOrderBook(TradingPair pair) override;
    bool getOrderBookSnapshot(TradingPair pair) override;
    std::string getExchangeName() const override { return m_name; }
    ExchangeId getExchangeId() const override { return m_id; }
    TradingPair symbolToTradingPair(const std::string& symbol) const override;
    std::string tradingPairToSymbol(TradingPair pair) const override;
    bool isConnected() const override { return m_connected; }

    // Callback setters
    void setSubscriptionCallback(std::function<void(bool)> callback) { m_subscriptionCallback = callback; }
    void setUpdateCallback(std::function<void()> callback) { m_updateCallback = callback; }
    void setSnapshotCallback(std::function<void(bool)> callback) { m_snapshotCallback = callback; }

    // Test control methods
    void sendOrderBookUpdate(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);
    void sendOrderBookSnapshot(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);

private:
    void processMessage(const std::string& message);
    void onServerConnect();
    void onServerMessage(const std::string& message);

    std::string m_name;
    ExchangeId m_id;
    OrderBookManager& m_orderBookManager;
    bool m_connected{false};
    
    // Callbacks
    std::function<void(bool)> m_subscriptionCallback;
    std::function<void()> m_updateCallback;
    std::function<void(bool)> m_snapshotCallback;
    
    // WebSocket server
    net::io_context m_ioc;
    std::unique_ptr<MockWebSocketServer> m_server;
    std::unique_ptr<websocket::stream<tcp::socket>> m_ws;
    std::thread m_ioThread;
    
    // Symbol mapping
    std::map<TradingPair, std::string> m_symbolMap;
}; 