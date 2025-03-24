#pragma once

#include "types.h"
#include "trading_pair_format.h"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>

// Price level in the order book
struct PriceLevel {
    double price;
    double quantity;

    PriceLevel(double p = 0.0, double q = 0.0) : price(p), quantity(q) {}
};

// Order book for a specific trading pair
class OrderBook {
public:
    OrderBook() = default;
    ~OrderBook() = default;

    // Update the order book with new price levels
    void update(const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
        std::lock_guard<std::mutex> lock(mutex);
        this->bids = bids;
        this->asks = asks;
    }

    // Get best bid price
    double getBestBid() const {
        std::lock_guard<std::mutex> lock(mutex);
        return bids.empty() ? 0.0 : bids[0].price;
    }

    // Get best ask price
    double getBestAsk() const {
        std::lock_guard<std::mutex> lock(mutex);
        return asks.empty() ? 0.0 : asks[0].price;
    }

    // Get bid quantity at best price
    double getBestBidQuantity() const {
        std::lock_guard<std::mutex> lock(mutex);
        return bids.empty() ? 0.0 : bids[0].quantity;
    }

    // Get ask quantity at best price
    double getBestAskQuantity() const {
        std::lock_guard<std::mutex> lock(mutex);
        return asks.empty() ? 0.0 : asks[0].quantity;
    }

    void updatePriceLevel(bool isBid, double price, double quantity);
    void updateBids(const std::vector<PriceLevel>& bids);
    void updateAsks(const std::vector<PriceLevel>& asks);
    
    const std::vector<PriceLevel>& getBids() const { return bids; }
    const std::vector<PriceLevel>& getAsks() const { return asks; }

private:
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    mutable std::mutex mutex;
};

// Order book manager for all trading pairs
class OrderBookManager {
public:
    OrderBookManager() = default;

    // Update order book for a trading pair
    void updateOrderBook(TradingPair pair, const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks) {
        std::lock_guard<std::mutex> lock(mutex);
        orderBooks[pair].update(bids, asks);
        
        // Notify callback if set
        if (updateCallback) {
            updateCallback(pair, orderBooks[pair]);
        }
    }

    // Get order book for a trading pair
    OrderBook& getOrderBook(TradingPair pair) {
        std::lock_guard<std::mutex> lock(mutex);
        return orderBooks[pair];
    }

    // Set callback for order book updates
    void setUpdateCallback(std::function<void(TradingPair, const OrderBook&)> callback) {
        std::lock_guard<std::mutex> lock(mutex);
        updateCallback = callback;
    }

private:
    std::map<TradingPair, OrderBook> orderBooks;
    std::function<void(TradingPair, const OrderBook&)> updateCallback;
    mutable std::mutex mutex;
};

// Stream operator for TradingPair - will use Binance's format for tracing
inline std::ostream& operator<<(std::ostream& os, TradingPair pair) {
    os << trading::toString(pair);
    return os;
} 