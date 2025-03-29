#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include "tracer.h"
#include "types.h"

struct PriceLevel {
    double price;
    double quantity;
    PriceLevel(double p = 0.0, double q = 0.0) : price(p), quantity(q) {}
};

// Order book for a specific trading pair
class OrderBook : public Traceable {
public:
    OrderBook() : exchangeId(ExchangeId::UNKNOWN), pair(TradingPair::UNKNOWN) {}
    OrderBook(ExchangeId exchangeId, TradingPair pair) 
        : exchangeId(exchangeId), pair(pair) {}
    ~OrderBook() = default;

    // Update the order book with new price levels
    void update(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);

    // Get best bid price
    double getBestBid() const {
        MUTEX_LOCK(mutex);
        return bids.empty() ? 0.0 : bids[0].price;
    }

    // Get best ask price
    double getBestAsk() const {
        MUTEX_LOCK(mutex);
        return asks.empty() ? 0.0 : asks[0].price;
    }

    // Get bid quantity at best price
    double getBestBidQuantity() const {
        MUTEX_LOCK(mutex);
        return bids.empty() ? 0.0 : bids[0].quantity;
    }

    // Get ask quantity at best price
    double getBestAskQuantity() const {
        MUTEX_LOCK(mutex);
        return asks.empty() ? 0.0 : asks[0].quantity;
    }

    // Get best prices and quantities atomically
    struct BestPrices {
        double bidPrice;
        double askPrice;
        double bidQuantity;
        double askQuantity;
    };

    BestPrices getBestPrices() const {
        MUTEX_LOCK(mutex);
        return BestPrices{
            bids.empty() ? 0.0 : bids[0].price,
            asks.empty() ? 0.0 : asks[0].price,
            bids.empty() ? 0.0 : bids[0].quantity,
            asks.empty() ? 0.0 : asks[0].quantity
        };
    }

    // Get last update timestamp
    std::chrono::system_clock::time_point getLastUpdate() const {
        MUTEX_LOCK(mutex);
        return lastUpdate;
    }

    // Get order book data atomically
    OrderBookData getOrderBookData() const {
        MUTEX_LOCK(mutex);
        return OrderBookData{
            bids.empty() ? 0.0 : bids[0].price,
            asks.empty() ? 0.0 : asks[0].price,
            bids.empty() ? 0.0 : bids[0].quantity,
            asks.empty() ? 0.0 : asks[0].quantity,
            lastUpdate
        };
    }
    
    // Get a copy of the current state atomically
    std::pair<std::vector<PriceLevel>, std::vector<PriceLevel>> getState() const {
        MUTEX_LOCK(mutex);
        return {bids, asks};
    }
    
    // Get a copy of current bids atomically
    std::vector<PriceLevel> getBids() const { 
        MUTEX_LOCK(mutex);
        return bids; 
    }

    // Get a copy of current asks atomically
    std::vector<PriceLevel> getAsks() const { 
        MUTEX_LOCK(mutex);
        return asks; 
    }

    void updatePriceLevel(bool isBid, double price, double quantity);

    // For TRACE identification
    ExchangeId getExchangeId() const { return exchangeId; }
    TradingPair getTradingPair() const { return pair; }

protected:
    void trace(std::ostream& os) const override {
        os << "OrderBook(" << exchangeId << ", " << pair << ")";
    }

private:
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    mutable std::mutex mutex;
    ExchangeId exchangeId;
    TradingPair pair;
    std::chrono::system_clock::time_point lastUpdate = std::chrono::system_clock::now();
    
    // Helper function to check if best bid/ask changed
    bool hasBestPricesChanged(bool isBid, double price, double quantity) const;
};

// Order book manager for all trading pairs
class OrderBookManager : public Traceable {
public:
    OrderBookManager();
    ~OrderBookManager() = default;

    // Update order book for a trading pair
    void updateOrderBook(ExchangeId exchangeId, TradingPair pair, const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);
    void updateOrderBook(ExchangeId exchangeId, TradingPair pair, double price, double quantity, bool isBid);

    // Get order book for a trading pair
    OrderBook& getOrderBook(TradingPair pair);

    // Set callback for order book updates
    void setUpdateCallback(std::function<void(ExchangeId, TradingPair, const OrderBook&)> callback);

protected:
    void trace(std::ostream& os) const override {
        os << "OrderBookManager()";
    }

private:
    std::map<TradingPair, OrderBook> orderBooks;
    std::function<void(ExchangeId, TradingPair, const OrderBook&)> updateCallback;
    mutable std::mutex mutex;
}; 