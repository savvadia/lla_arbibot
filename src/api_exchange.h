#pragma once

#include "types.h"
#include "orderbook.h"
#include <string>
#include <functional>
#include <memory>
#include "tracer.h"

// Base class for exchange APIs
class ApiExchange : public Traceable {
public:
    virtual ~ApiExchange() = default;

    // Connect to the exchange
    virtual bool connect() = 0;

    // Disconnect from the exchange
    virtual void disconnect() = 0;

    // Subscribe to order book updates for a trading pair
    virtual bool subscribeOrderBook(TradingPair pair) = 0;

    // Get order book snapshot for a trading pair
    virtual bool getOrderBookSnapshot(TradingPair pair) = 0;

    // Order management
    virtual bool placeOrder(TradingPair pair, OrderType type, double price, double quantity) = 0;
    virtual bool cancelOrder(const std::string& orderId) = 0;
    virtual bool getBalance(const std::string& asset) = 0;

    // Get exchange name
    virtual std::string getExchangeName() const = 0;

    // Get exchange ID
    virtual ExchangeId getExchangeId() const = 0;

    // Check if connected to the exchange
    virtual bool isConnected() const = 0;

    // Callback setters
    virtual void setSubscriptionCallback(std::function<void(bool)> callback) = 0;
    virtual void setSnapshotCallback(std::function<void(bool)> callback) = 0;

    // operator<<
    friend std::ostream& operator<<(std::ostream& os, const ApiExchange& api) {
        api.trace(os);
        return os;
    }

    void trace(std::ostream& os) const override {
        os << "Api" << getExchangeName();
    }

protected:
    // Helper function to convert string to lowercase
    static std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }
};

// Factory function to create exchange API instances
std::unique_ptr<ApiExchange> createApiExchange(const std::string& exchangeName, OrderBookManager& orderBookManager); 