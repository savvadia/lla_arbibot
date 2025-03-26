#pragma once

#include "types.h"
#include "orderbook.h"
#include <string>
#include <functional>
#include <memory>
#include "tracer.h"

// Base class for exchange APIs
class ExchangeApi : public Traceable {
public:
    virtual ~ExchangeApi() = default;

    // Connect to the exchange
    virtual bool connect() = 0;

    // Disconnect from the exchange
    virtual void disconnect() = 0;

    // Subscribe to order book updates for a trading pair
    virtual bool subscribeOrderBook(TradingPair pair) = 0;

    // Get order book snapshot for a trading pair
    virtual bool getOrderBookSnapshot(TradingPair pair) = 0;

    // Get exchange name
    virtual std::string getExchangeName() const = 0;

    // Get exchange ID
    virtual ExchangeId getExchangeId() const = 0;

    // Convert exchange-specific symbol to TradingPair
    virtual TradingPair symbolToTradingPair(const std::string& symbol) const = 0;

    // Convert TradingPair to exchange-specific symbol
    virtual std::string tradingPairToSymbol(TradingPair pair) const = 0;

    // Check if connected to the exchange
    virtual bool isConnected() const = 0;

    // operator<<
    friend std::ostream& operator<<(std::ostream& os, const ExchangeApi& api) {
        api.trace(os);
        return os;
    }

    void trace(std::ostream& os) const override {
        os << getExchangeName();
    }

protected:
    // Helper function to convert string to lowercase
    static std::string toLower(std::string str) {
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    }
};

// Factory function to create exchange API instances
std::unique_ptr<ExchangeApi> createExchangeApi(const std::string& exchangeName, OrderBookManager& orderBookManager); 