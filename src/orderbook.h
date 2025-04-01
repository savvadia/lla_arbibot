#pragma once

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <chrono>
#include <functional>
#include "tracer.h"
#include "types.h"
#include <unordered_map>
#include "config.h"

struct PriceLevel {
    double price;
    double quantity;
    PriceLevel(double p = 0.0, double q = 0.0) : price(p), quantity(q) {}
};

// Helper function to merge sorted lists while maintaining size limit
void mergeSortedLists(std::vector<PriceLevel>& oldList, const std::vector<PriceLevel>& newList, 
                     bool isBid, double& totalAmount, double limit = Config::MAX_ORDER_BOOK_AMOUNT);

// Order book for a specific trading pair
class OrderBook : public Traceable {
public:
    OrderBook() : exchangeId(ExchangeId::UNKNOWN), pair(TradingPair::UNKNOWN) {}
    OrderBook(ExchangeId exchangeId, TradingPair pair) 
        : exchangeId(exchangeId), pair(pair) {}
    
    // Copy constructor
    OrderBook(const OrderBook& other) 
        : exchangeId(other.exchangeId), pair(other.pair), lastUpdate(other.lastUpdate) {
        std::lock_guard<std::mutex> lock(other.mutex);
        bids = other.bids;
        asks = other.asks;
    }
    
    // Assignment operator
    OrderBook& operator=(const OrderBook& other) {
        if (this != &other) {
            std::lock_guard<std::mutex> lock(mutex);
            std::lock_guard<std::mutex> otherLock(other.mutex);
            exchangeId = other.exchangeId;
            pair = other.pair;
            bids = other.bids;
            asks = other.asks;
            lastUpdate = other.lastUpdate;
        }
        return *this;
    }
    
    ~OrderBook() = default;

    // Update the order book with new price levels (assumes sorted input)
    void update(const std::vector<PriceLevel>& newBids, const std::vector<PriceLevel>& newAsks);

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

    // Get worst bid price (lowest)
    double getWorstBid() const {
        MUTEX_LOCK(mutex);
        return bids.empty() ? 0.0 : bids.back().price;
    }

    // Get worst ask price (highest)
    double getWorstAsk() const {
        MUTEX_LOCK(mutex);
        return asks.empty() ? 0.0 : asks.back().price;
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

    // Get bid quantity at worst price
    double getWorstBidQuantity() const {
        MUTEX_LOCK(mutex);
        return bids.empty() ? 0.0 : bids.back().quantity;
    }

    // Get ask quantity at worst price
    double getWorstAskQuantity() const {
        MUTEX_LOCK(mutex);
        return asks.empty() ? 0.0 : asks.back().quantity;
    }

    // Get best prices and quantities atomically
    struct BestPrices {
        double bestBid;
        double bestAsk;
        double worstBid;
        double worstAsk;
        double bestBidQuantity;
        double bestAskQuantity;
        double worstBidQuantity;
        double worstAskQuantity;
    };

    BestPrices getBestPrices() const {
        MUTEX_LOCK(mutex);
        return BestPrices{
            bids.empty() ? 0.0 : bids[0].price,
            asks.empty() ? 0.0 : asks[0].price,
            bids.empty() ? 0.0 : bids.back().price,
            asks.empty() ? 0.0 : asks.back().price,
            bids.empty() ? 0.0 : bids[0].quantity,
            asks.empty() ? 0.0 : asks[0].quantity,
            bids.empty() ? 0.0 : bids.back().quantity,
            asks.empty() ? 0.0 : asks.back().quantity
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

    // Helper function to check if best/worst prices changed
    bool hasPricesChanged(const BestPrices& oldPrices, const BestPrices& newPrices) const;

protected:
    void trace(std::ostream& os) const override {
        MUTEX_LOCK(mutex);
        os << exchangeId << " " << pair << " "
           << std::fixed << std::setprecision(2)
           << "bid:" << (bids.empty() ? 0.0 : bids[0].price) << "-" << (bids.empty() ? 0.0 : bids.back().price)
           << " ask:" << (asks.empty() ? 0.0 : asks[0].price) << "-" << (asks.empty() ? 0.0 : asks.back().price)
           << " " << std::chrono::duration_cast<std::chrono::microseconds>(lastUpdate.time_since_epoch()).count();
    }

private:
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    mutable std::mutex mutex;
    ExchangeId exchangeId;
    TradingPair pair;
    std::chrono::system_clock::time_point lastUpdate = std::chrono::system_clock::now();
};

// Order book manager for all trading pairs
class OrderBookManager : public Traceable {
public:
    OrderBookManager();
    ~OrderBookManager() = default;

    // Update order book for a trading pair
    void updateOrderBook(ExchangeId exchangeId, TradingPair pair, const std::vector<PriceLevel>& bids, const std::vector<PriceLevel>& asks);

    // Get order book for a specific exchange and trading pair
    OrderBook& getOrderBook(ExchangeId exchangeId, TradingPair pair);

    // Get all order books for a trading pair
    std::vector<std::reference_wrapper<OrderBook>> getOrderBooks(TradingPair pair);

    // Get all order books for an exchange
    std::vector<std::reference_wrapper<OrderBook>> getOrderBooks(ExchangeId exchangeId);

    // Set callback for order book updates
    void setUpdateCallback(std::function<void(ExchangeId, TradingPair)> callback);

protected:
    void trace(std::ostream& os) const override {
        os << "OrderBookMgr";
    }

private:
    // Map of exchange ID to map of trading pair to order book
    std::unordered_map<ExchangeId, std::unordered_map<TradingPair, OrderBook>> orderBooks;
    std::function<void(ExchangeId, TradingPair)> updateCallback;
    mutable std::mutex mutex;
}; 