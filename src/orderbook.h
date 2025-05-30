#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <chrono>
#include <cassert>
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
    
    // Copy constructor
    OrderBook(const OrderBook& other) 
        : exchangeId(other.exchangeId), pair(other.pair), lastUpdate(other.lastUpdate) {
        MUTEX_LOCK(other.mutex);
        bids = other.bids;
        asks = other.asks;
    }
    
    // Assignment operator
    OrderBook& operator=(const OrderBook& other) {
        if (this != &other) {
            MUTEX_LOCK(mutex);
            MUTEX_LOCK(other.mutex);
            exchangeId = other.exchangeId;
            pair = other.pair;
            bids = other.bids;
            asks = other.asks;
            lastUpdate = other.lastUpdate;
        }
        return *this;
    }
    
    ~OrderBook() = default;

    enum class UpdateOutcome {
        BEST_PRICES_CHANGED,
        NO_CHANGES_TO_BEST_PRICES,
        UPDATE_ERROR
    };

    // Update the order book with new price levels (assumes sorted input)
    UpdateOutcome update(TradingPair pair,
                        std::vector<PriceLevel>& newBids,
                        std::vector<PriceLevel>& newAsks,
                        bool isCompleteUpdate = false, int maxDepth = 10);

    // Set best bid and ask prices directly (for bookTicker style updates)
    UpdateOutcome setBestBidAsk(double bidPrice, double bidQuantity, double askPrice, double askQuantity);

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
    struct BestPrices: public Traceable {
        double bestBid;
        double bestAsk;
        double worstBid;
        double worstAsk;
        double bestBidQuantity;
        double bestAskQuantity;
        double worstBidQuantity;
        double worstAskQuantity;

        BestPrices(double bestBid, double bestAsk, double worstBid, double worstAsk, double bestBidQuantity, double bestAskQuantity, double worstBidQuantity, double worstAskQuantity)
            : bestBid(bestBid), bestAsk(bestAsk), worstBid(worstBid), worstAsk(worstAsk), bestBidQuantity(bestBidQuantity), bestAskQuantity(bestAskQuantity), worstBidQuantity(worstBidQuantity), worstAskQuantity(worstAskQuantity) {}

        void trace(std::ostream& os) const override {
            os << bestBid << "-" << bestAsk << " ";
        }
    };

    BestPrices getBestPrices() const {
        MUTEX_LOCK(mutex);
        BestPrices bp =  BestPrices(
            bids.empty() ? 0.0 : bids[0].price,
            asks.empty() ? 0.0 : asks[0].price,
            bids.empty() ? 0.0 : bids.back().price,
            asks.empty() ? 0.0 : asks.back().price,
            bids.empty() ? 0.0 : bids[0].quantity,
            asks.empty() ? 0.0 : asks[0].quantity,
            bids.empty() ? 0.0 : bids.back().quantity,
            asks.empty() ? 0.0 : asks.back().quantity);
        return bp;
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
        // assert that bids and asks are sorted
        assert(isSorted(bids, true));
        assert(isSorted(asks, false));
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

    // For TRACE identification
    ExchangeId getExchangeId() const { return exchangeId; }
    TradingPair getTradingPair() const { return pair; }

    // Helper function to check if best/worst prices changed
    bool hasPricesChanged(const BestPrices& oldPrices, const BestPrices& newPrices) const;

    static bool isSorted(const std::vector<PriceLevel>& list, bool isBid);
    static void sortList(std::vector<PriceLevel>& list, bool isBid);

    void mergeSortedLists(std::vector<PriceLevel>& oldList, std::vector<PriceLevel>& newList, bool isBid);

    void trace(std::ostream& os) const override {
        os << pair << " " << bids.size() << "/" << asks.size() << " " << std::fixed << std::setprecision(3) <<
            getBestPrices() << "u: " << (lastUpdate);
    }
    std::string traceBidsAsks(std::vector<PriceLevel>& list) const {
        std::stringstream ss;
        auto precision = TradingPairData::getPrecision(pair);
        ss << std::fixed;
        ss << "[";
        for(const auto& entry : list) {
            ss << std::setprecision(precision) << entry.price << "/" << std::setprecision(8) << entry.quantity << " ";
        }
        ss << "]";
        return ss.str();
    }

protected:

private:
    std::vector<PriceLevel> bids;
    std::vector<PriceLevel> asks;
    mutable std::mutex mutex;
    ExchangeId exchangeId;
    TradingPair pair;

    // lastUpdate is the timestamp of the last update; not the "u" field in the exchange upadate message
    std::chrono::system_clock::time_point lastUpdate = std::chrono::system_clock::now();

    void pushElement(std::vector<PriceLevel>& result, std::vector<PriceLevel>::iterator& it, int scenario);
};
