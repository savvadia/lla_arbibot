#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
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
    
    // Get a copy of the current state
    std::pair<std::vector<PriceLevel>, std::vector<PriceLevel>> getState() const {
        std::lock_guard<std::mutex> lock(mutex);
        return {bids, asks};
    }
    
    const std::vector<PriceLevel>& getBids() const { 
        std::lock_guard<std::mutex> lock(mutex);
        return bids; 
    }
    const std::vector<PriceLevel>& getAsks() const { 
        std::lock_guard<std::mutex> lock(mutex);
        return asks; 
    }

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
};

// Order book manager for all trading pairs
class OrderBookManager {
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

private:
    std::map<TradingPair, OrderBook> orderBooks;
    std::function<void(ExchangeId, TradingPair, const OrderBook&)> updateCallback;
    mutable std::mutex mutex;
}; 